#include "mitoshooks.h"

#include "Mitos.h"
#include "virtual_address_writer.h"

#include <stdio.h>
#include <dlfcn.h>

#include <unistd.h>
#include <sys/syscall.h>

// #define _GNU_SOURCE // sched_getcpu(3) is glibc-specific (see the man page)
#include <sched.h>
#include <cassert>
#include <ctime>

// 512 should be enough for xeon-phi
#define MAX_THREADS 512
#define DEFAULT_PERIOD      4000

struct func_args
{
    void *(*func)(void*);
    void *args;
};

// void* routine_wrapper(void *args)
// {
//     func_args *routine_struct = (func_args*)args;
//
//     Mitos_begin_sampler();
//
//     return routine_struct->func(routine_struct->args);
// }
//
// // pthread hooks
// int pthread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine) (void*), void *arg)
// {
//     //fprintf(stderr, "pthread_create hook\n");
//     static pthread_create_fn_t og_pthread_create = NULL;
//     if(!og_pthread_create)
//         og_pthread_create = (pthread_create_fn_t)dlsym(RTLD_NEXT, "pthread_create");
//
//     struct func_args *f = (struct func_args*)malloc(sizeof(struct func_args));
//     f->func = start_routine;
//     f->args = arg;
//
//     return og_pthread_create(thread, attr, routine_wrapper, f);
// }
//
// void pthread_exit(void *retval)
// {
//     //fprintf(stderr, "pthread_exit hook\n");
//     static pthread_exit_fn_t og_pthread_exit = NULL;
//     if(!og_pthread_exit)
//         og_pthread_exit = (pthread_exit_fn_t)dlsym(RTLD_NEXT, "pthread_exit");
//
//     Mitos_end_sampler();
//
//     og_pthread_exit(retval);
// }
mitos_output mout;

#ifdef USE_MPI
// MPI hooks
long ts_output = 0;

void sample_handler(perf_event_sample *sample, void *args)
{
//    fprintf(stderr, "MPI handler sample: cpu=%d, tid=%d\n", sample->cpu, sample->tid);
    Mitos_write_sample(sample, &mout);
}

int MPI_Init(int *argc, char ***argv)
{
    fprintf(stderr, "MPI_Init hook\n");
    int ret = PMPI_Init(argc, argv);

    int mpi_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    ts_output = std::time(NULL);
    MPI_Bcast(&ts_output, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    // send timestamp from rank 0 to all others to synchronize folder prefix

    char rank_prefix[48];
    sprintf(rank_prefix, "%ld_rank_%d", ts_output, mpi_rank);

    if (mpi_rank == 0) {
        save_virtual_address_offset("virt_address.txt");
    }
    Mitos_create_output(&mout, rank_prefix);
    pid_t curpid = getpid();
    std::cout << "Curpid: " << curpid << ", Rank: " << mpi_rank << std::endl;

    Mitos_pre_process(&mout);
    Mitos_set_pid(curpid);

    Mitos_set_handler_fn(&sample_handler,NULL);
    Mitos_set_sample_latency_threshold(3);
    Mitos_set_sample_event_period(DEFAULT_PERIOD);
    Mitos_set_sample_time_frequency(4000);
    Mitos_begin_sampler();

    return ret;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided)
{
    fprintf(stderr, "MPI_Init_thread hook\n");
    int ret = PMPI_Init_thread(argc, argv, required, provided);

    int mpi_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    char rank_prefix[32];
    sprintf(rank_prefix, "rank_%d", mpi_rank);

    Mitos_create_output(&mout, rank_prefix);
    Mitos_pre_process(&mout);

    Mitos_set_handler_fn(&sample_handler,NULL);
    Mitos_set_sample_latency_threshold(3);
    Mitos_set_sample_time_frequency(4000);
    Mitos_begin_sampler();

    return ret;
}

int MPI_Finalize()
{
    std::cout << "MPI Finalize\n";
    //fprintf(stderr, "MPI_Finalize hook\n");
    int mpi_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    Mitos_end_sampler();
    Mitos_post_process("/proc/self/exe", &mout);
    MPI_Barrier(MPI_COMM_WORLD);
    // merge files
    if (mpi_rank == 0) {
        int ret_val = Mitos_merge_files(std::to_string(ts_output) + "_rank_", std::to_string(ts_output) + "_rank_0");
    }
    return PMPI_Finalize();
}
#endif // USE_MPI


#ifdef USE_OPEN_MP


void sample_handler_omp(perf_event_sample *sample, void *args)
{
//    fprintf(stderr, "MPI handler sample: cpu=%d, tid=%d\n", sample->cpu, sample->tid);
    Mitos_write_sample(sample, &mout);
}


#define register_callback_t(name, type)                                        \
  do {                                                                         \
    type f_##name = &on_##name;                                                \
    if (ompt_set_callback(name, (ompt_callback_t)f_##name) == ompt_set_never)  \
      printf("0: Could not register callback '" #name "'\n");                  \
  } while (0)

#define register_callback(name) register_callback_t(name, name##_t)

static uint64_t my_next_id() {
    static uint64_t ID = 0;
    uint64_t ret = __sync_fetch_and_add(&ID, 1);
    assert(ret < MAX_THREADS &&
           "Maximum number of allowed threads is limited by MAX_THREADS");
    return ret;
}



static void on_ompt_callback_thread_begin(ompt_thread_t thread_type,
                                          ompt_data_t *thread_data) {
    uint64_t tid_omp = thread_data->value = my_next_id();
    //counter[tid].cc.thread_begin += 1;
//#ifdef SYS_gettid
//    pid_t tid = syscall(SYS_gettid);
//#else
//#error "SYS_gettid unavailable on this system"
//    exit(1);
//#endif
//    int cpu_num = sched_getcpu();
//    printf("Start Thread OMP: %u, tid: %u, omp_tid: %lu, cpu_id: %u\n",getpid(), tid, tid_omp, cpu_num);

}

static void on_ompt_callback_thread_end(ompt_data_t *thread_data) {
    uint64_t tid_omp = thread_data->value;
    //counter[tid].cc.thread_end += 1;
//#ifdef SYS_gettid
//    pid_t tid = syscall(SYS_gettid);
//#else
//#error "SYS_gettid unavailable on this system"
//#endif
//     printf("End Thread OMP: %u, tid: %u, omp_tid: %lu\n",getpid(), tid, tid_omp);
}

int ompt_initialize(ompt_function_lookup_t lookup, int initial_device_num,
                    ompt_data_t *tool_data) {
    printf("libomp init time: %f\n",
           omp_get_wtime() - *(double *) (tool_data->ptr));
    *(double *) (tool_data->ptr) = omp_get_wtime();
    // initialize callback
    ompt_set_callback_t ompt_set_callback =
            (ompt_set_callback_t)lookup("ompt_set_callback");
    register_callback(ompt_callback_thread_begin);
    register_callback(ompt_callback_thread_end);

    save_virtual_address_offset("virt_address.txt");
    Mitos_create_output(&mout, "openmp_all");
//    pid_t curpid = getpid();
//    std::cout << "Curpid: " << curpid << ", Rank: " << std::endl;

    Mitos_pre_process(&mout);
    Mitos_set_pid(getpid());

    Mitos_set_handler_fn(&sample_handler_omp,NULL);
    Mitos_set_sample_latency_threshold(3);
    Mitos_set_sample_event_period(DEFAULT_PERIOD);
    Mitos_set_sample_time_frequency(4000);
    Mitos_begin_sampler();
    std::cout << "Begin sampling: " << getpid() << "\n";


    return 1; // success: activates tool
}

void ompt_finalize(ompt_data_t *tool_data) {
    printf("[OMP Finalize] application runtime: %f\n",
           omp_get_wtime() - *(double *) (tool_data->ptr));

    printf("End Sampler...\n");
    Mitos_end_sampler();
    printf("Post process...\n");
    // ./../examples/omp_example
    // /proc/self/exe
    // TODO: valid bin_name leads to an infinite loop in Symtab::SymtabAPI::openFile(bin_name, ...)
    Mitos_post_process("", &mout);

}

// only used for debugging purposes
//void test_symtab() {
//    SymtabAPI::Symtab *symtab_obj;
//    SymtabCodeSource *symtab_code_src;
//    std::cout << "Symtab: Open File..." << "\n";
//    int sym_success = SymtabAPI::Symtab::openFile(symtab_obj,"/proc/self/exe");
//    std::cout << "Completed: " << sym_success << "\n";
//}

#ifdef __cplusplus
extern "C" {
#endif
ompt_start_tool_result_t *ompt_start_tool(unsigned int omp_version,
                                          const char *runtime_version) {
    static double time = 0; // static defintion needs constant assigment
    time = omp_get_wtime();
    //printf("Init_start_tool: %u \n", getpid());

    static ompt_start_tool_result_t ompt_start_tool_result = {
            &ompt_initialize, &ompt_finalize, {.ptr = &time}};
    return &ompt_start_tool_result; // success: registers tool
}
#ifdef __cplusplus
}
#endif

#endif