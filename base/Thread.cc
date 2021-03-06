//
// Created by renz2048 on 2020/7/8.
//

#include "Thread.h"
#include "CurrentThread.h"
#include "Timestamp.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>

namespace fakeMuduo
{
    namespace detail
    {
        pid_t gettid()
        {
            /**
             * syscall 执行一个系统调用，所使用的符号常量可以在头文件 <sys/syscall.h> 里面找到
             *
             * linux 中每个进程有一个 pid（pid_t），由 getpid() 取得。
             * linux 中 POSIX 线程 也有一个 id，类型 pthread_t，由 pthread_self() 取得，
             *   其 id 空间是各个进程独立的（即不同进程中的线程可能有相同的 id）。
             *   若进程P1 要向另外一个进程P2 中的某个线程发送信号，
             *   既不能使用 P2 的 pid，也不能使用线程的 pthread_id，
             *   只能使用线程的真实pid，即 tid
             *
             * tid 可以通过 gettid() 得到，但 glibc 并没有实现该函数，
             *   只能通过 linxu 的系统调用 syscall 来获取
             */
            return static_cast<pid_t>(::syscall(SYS_gettid));
        }

        void afterFork()
        {
            fakeMuduo::CurrentThread::t_cachedTid = 0;
            fakeMuduo::CurrentThread::t_threadName = "main";
            CurrentThread::tid();
        }

        class ThreadNameInitializer
        {
        public:
            ThreadNameInitializer()
            {
                fakeMuduo::CurrentThread::t_threadName = "main";
                CurrentThread::tid();
                pthread_atfork(NULL, NULL, &afterFork);
            }
        };

        ThreadNameInitializer init;

        struct ThreadData
        {
            typedef fakeMuduo::Thread::ThreadFunc ThreadFunc;
            ThreadFunc func_;
            string name_;
            pid_t* tid_;

            ThreadData(ThreadFunc func,
                       const string& name,
                       pid_t* tid)
                       : func_(std::move(func)),
                         name_(name),
                         tid_(tid)
            {}

            void runInThread()
            {
                *tid_ = fakeMuduo::CurrentThread::tid();
                tid_ = NULL;

                fakeMuduo::CurrentThread::t_threadName = name_.empty() ? "fakeMuduoThread" : name_.c_str();
                /*
                 * #include <sys/prctl.h>
                 * int prctl(int option, unsigned long arg2, unsinged long arg3, unsigned long arg4, unsigned long arg5);
                 * PR_SET_NAME：设置进程名
                 */
                ::prctl(PR_SET_NAME, fakeMuduo::CurrentThread::t_threadName);

                func_();
                fakeMuduo::CurrentThread::t_threadName = "finished";
            }
        };

        void* startThread(void* obj)
        {
            ThreadData* data = static_cast<ThreadData*>(obj);
            data->runInThread();
            delete data;
            return NULL;
        }
    } // namespace detail

    /**
     * 获取当前线程的pid，存放在 t_cachedTid
     * 转换为字符串格式，存放在t_tidString
     * 长度存放在t_tidStringLength
     */
    void CurrentThread::cacheTid() {
        if (t_cachedTid == 0) {
            t_cachedTid = detail::gettid();
            t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
        }
    }

    /**
     * 当前线程的真实pid由tid()获得，getpid获得主线程的pid
     * @return
     */
    bool CurrentThread::isMainThread() {
        return tid() == ::getpid();
    }

    /**
     * 暂停调用线程的执行，知道 usec 时间后恢复
     * 也可能因为其它信号触发了线程中的处理程序的调用
     */
    void CurrentThread::sleepUsec(int64_t usec) {
        struct timespec ts = {0, 0};
        ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
        ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
        ::nanosleep(&ts, NULL);
    }

    AtomicInt32 Thread::numCreated_;

    Thread::Thread(ThreadFunc func, const string &n)
      : started_(false),
        joined_(false),
        pthreadId_(0),
        tid_(0),
        func_(std::move(func)),
        name_(n)
    {
        setDefaultName();
    }

    Thread::~Thread()
    {
        if (started_ && !joined_)
        {
            pthread_detach(pthreadId_);
        }
    }

    void Thread::setDefaultName() {
        int num = numCreated_.incrementAndGet();
        if (name_.empty()) {
            char buf[32];
            snprintf(buf, sizeof buf, "Thread%d", num);
            name_ = buf;
        }
    }

    void Thread::start() {
        assert(!started_); // started_ 变量必须为 false
        started_ = true;
        detail::ThreadData* data = new detail::ThreadData(func_, name_, &tid_);
        /*
         * 创建新线程
         * #include <pthread.h>
         * int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
         *                      void *(*start_routine) (void *), void *arg);
         * compile and link with -pthread
         * - thread：线程 ID
         * - attr：设置线程属性
         * - start_routine：新线程从 start_routine 函数开始执行
         * - arg：传递给 start_routine 的参数
         * 成功返回0。
         */
        printf("Thread::start : (before pthread_create) tid_=%d\n", tid_);
        printf("Thread::start : data->tid_=%d\n", data->tid_);
        if (pthread_create(&pthreadId_, NULL, &detail::startThread, data)) {
            started_ = false;
            delete data;
        } else {
            printf("Thread::start : (after pthread_create) tid_=%d\n", tid_);
            assert(tid_ > 0);
        }
    }

    int Thread::join() {
        assert(started_);
        assert(!joined_);
        joined_ = true;
        return pthread_join(pthreadId_, NULL);
    }
}