//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/task.cpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/task.cpp,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


#include "isptech/concurrency/task.hpp"
#include <algorithm>
#include <windows.h>

#pragma warning(disable: 4073)
#pragma init_seg(lib)


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Names/Types
*/
using std::count_if;
using std::find_if;
using std::move;
using std::sort;
using std::try_to_lock;


/*
    Data
*/
Scheduler scheduler(1);


/*
    Task Channel Selection Lock Guard
*/
Task::Channel_selection::Lock_guard::Lock_guard(Channel_operation* fst, Channel_operation* lst)
    : first(fst)
    , last(lst)
{
    Channel_base* prevchanp = nullptr;

    for (Channel_operation* cop = first; cop != last; ++cop) {
        Channel_base* chanp = cop->channel();
        if (chanp && chanp != prevchanp) {
            chanp->lock();
            prevchanp = chanp;
        }
    }
}


Task::Channel_selection::Lock_guard::~Lock_guard()
{
    Channel_base* prevchanp = nullptr;

    for (Channel_operation* cop = first; cop != last; ++cop) {
        Channel_base* chanp = cop->channel();
        if (chanp && chanp != prevchanp) {
            chanp->unlock();
            prevchanp = chanp;
        }
    }
}


/*
    Task Channel Selection Sort Guard
*/
inline
Task::Channel_selection::Sort_guard::Sort_guard(Channel_operation* begin, Channel_operation* end)
    : first(begin)
    , last(end)
{
    save_positions(first, last);
    sort_channels(first, last);
}


inline
Task::Channel_selection::Sort_guard::~Sort_guard()
{
    restore_positions(first, last);
}


inline void
Task::Channel_selection::Sort_guard::dismiss()
{
    first = last;
}


/*
    Task Channel Selection
*/
Channel_size
Task::Channel_selection::count_ready(const Channel_operation* first, const Channel_operation* last)
{
    return count_if(first, last, [](auto& co) {
        return co.is_ready();
    });
}


Channel_size
Task::Channel_selection::dequeue(Channel_operation* first, Channel_operation* last, Channel_size selected, Handle task)
{
    Channel_size n = 0;

    for (Channel_operation* cop = first; cop != last; ++cop) {
        if (cop->position() != selected && cop->dequeue(task))
            ++n;
    }

    return n;
}


Channel_size
Task::Channel_selection::enqueue(Channel_operation* first, Channel_operation* last, Handle task)
{
    Channel_size n = 0;

    for (Channel_operation* cop = first; cop != last; ++cop) {
        cop->enqueue(task);
        ++n;
    }

    return n;
}


Channel_operation*
Task::Channel_selection::pick_ready(Channel_operation* first, Channel_operation* last, Channel_size nready)
{
    Channel_operation*  readyp  = last;
    Channel_size        n       = random(1, nready);

    for (Channel_operation* cop = first; cop != last; ++cop) {
        if (cop->is_ready() && --n == 0) {
            readyp = cop;
            break;
        }
    }

    return readyp;
}


void
Task::Channel_selection::restore_positions(Channel_operation* first, Channel_operation* last)
{
    using std::sort;

    sort(first, last, [](auto& x, auto& y) {
        return x.position() < y.position();
    });
}


void
Task::Channel_selection::save_positions(Channel_operation* first, Channel_operation* last)
{
    for (Channel_operation* cop = first; cop != last; ++cop)
        cop->position(cop - first);
}


Task::Selection_status
Task::Channel_selection::select(Channel_size pos, Handle task)
{
    --nenqueued;
    if (!winner) {
        winner = pos;
        nenqueued -= dequeue(begin, end, pos, task);
    }

    return Selection_status(*winner, nenqueued == 0);
}


bool
Task::Channel_selection::select(Channel_operation* first, Channel_operation* last, Handle task)
{
    Sort_guard chansort{first, last};
    Lock_guard chanlocks{first, last};

    nenqueued   = 0;
    winner      = select_ready(first, last);

    // If nothing is ready, enqueue the operations on their channels and
    // keep them in lock order until we awake.
    if (!winner) {
        nenqueued = enqueue(first, last, task);
        chansort.dismiss();
        begin = first;
        end = last;
    }

    return winner ? true : false;
}


optional<Channel_size>
Task::Channel_selection::select_ready(Channel_operation* first, Channel_operation* last)
{
    optional<Channel_size>  pos;
    const Channel_size      n = count_ready(first, last);

    if (n > 0) {
        Channel_operation* cop = pick_ready(first, last, n);
        cop->execute();
        pos = cop->position();
    }

    return pos;
}

   
Channel_size
Task::Channel_selection::selected() const
{
    restore_positions(begin, end);
    return *winner;
}


inline void
Task::Channel_selection::sort_channels(Channel_operation* first, Channel_operation* last)
{
    using std::sort;

    sort(first, last, [](auto& x, auto& y) {
        return x.channel() < y.channel();
    });
}


template<Channel_size N>
optional<Channel_size>
Task::Channel_selection::try_select(Channel_operation (&ops)[N])
{
    Channel_operation*  first = begin(ops);
    Channel_operation*  last = end(ops);
    Sort_guard          chansort{first, last};
    Lock_guard          chanlocks{first, last};

    return select_ready(first, last);
}


/*
    Task Future Selection Channel Locks
*/
Task::Future_selection::Channel_locks::Channel_locks(const Channel_wait_vector* wsp)
    : waitsp{wsp}
{
    sort_channels(waitsp);
    for (auto& w : *waitsp)
        w.lock_channel();
}


Task::Future_selection::Channel_locks::~Channel_locks()
{
    for (auto& w : *waitsp)
        w.unlock_channel();
}


/*
    Task Future Selection Future Sort
*/
Task::Future_selection::Future_sort::Future_sort(Channel_wait_vector* wsp)
    : waitsp{wsp}
{
    sort(waitsp->begin(), waitsp->end(), [](auto& x, auto& y) {
        return x.future() < y.future();
    });
}


Task::Future_selection::Future_sort::~Future_sort()
{
    sort_channels(waitsp->begin(), waitsp->end());
}


/*
    Task Future Selection
*/
Channel_size
Task::Future_selection::count_ready(const Channel_wait_vector& ws)
{
    return count_if(ws.begin(), ws.end(), [](auto& w) {
        return w.is_ready();
    });
}


Channel_size
Task::Future_selection::dequeue_not_ready(Future_wait_vector* futsp, Channel_wait_vector* chansp, Handle task)
{
    using Vector_size = Future_wait_vector::size_type;

    Future_wait_vector&     futures = *futsp;
    Channel_wait_vector&    channels = *chansp;
    Channel_size            n = 0;

    for (Vector_size i = 0; i < futures.size(); ++i) {
        Future_wait& future = futures[i];
        if (!future.is_ready())
            future.dequeue(&channels);
    }

    return n;
}


Channel_size
Task::Future_selection::enqueue_not_ready(Future_wait_vector* futsp, Channel_wait_vector* chansp, Handle task)
{
    using Vector_size = Future_wait_vector::size_type;

    Future_wait_vector&     futures     = *futsp;
    Channel_wait_vector&    channels    = *chansp;
    Channel_size            n           = 0;

    for (Vector_size i = 0; i < futures.size(); ++i) {
        Future_wait& future = futures[i];

        if (future.is_ready(channels))
            future.complete();
        else {
            future.enqueue(task, &channels);
            ++n;
        }
    }

    return n;
}


Task::Future_selection::Channel_wait_vector::const_iterator
Task::Future_selection::pick_ready(const Channel_wait_vector& waits, Channel_size nready)
{
    auto            readyp  = waits.end();
    Channel_size    n       = random(1, nready);

    for (auto wp = waits.begin(); wp != waits.end(); ++wp) {
        if (wp->is_ready() && --n == 0) {
            readyp = wp;
            break;
        }
    }

    return readyp;
}


Task::Selection_status
Task::Future_selection::select_channel(Channel_size pos, Handle task)
{
    complete(&futures, &channels, pos, task);
    --nenqueued;

    if (!winner) {
        winner = channels[pos].future();
        if (type == Type::any)
            nenqueued -= dequeue_not_ready(&futures, &channels, task);
    }

    return Selection_status(*winner, nenqueued == 0);
}


optional<Channel_size>
Task::Future_selection::select_ready(const Channel_wait_vector& ws)
{
    optional<Channel_size>  ready;
    const Channel_size      n = count_ready(ws);

    if (n > 0) {
        auto wp = pick_ready(ws, n);
        ready = wp->future();
        wp->complete();
    }

    return ready;
}


void
Task::Future_selection::sort_channels(Channel_wait_vector* waitsp)
{
    std::sort(waitsp->begin(), waitsp->end(), [](auto& x, auto& y) {
        return x.channel() < y.channel();
    });
}


/*
    Task Promise
*/
inline void
Task::Promise::make_ready()
{
    taskstat = Status::ready;
}


inline Task::Status
Task::Promise::status() const
{
    return taskstat;
}


inline void
Task::Promise::unlock()
{
    mutex.unlock();
}


/*
    Task
*/
Channel_size
Task::random(Channel_size min, Channel_size max)
{
    using Device        = std::random_device;
    using Engine        = std::default_random_engine;
    using Distribution  = std::uniform_int_distribution<Channel_size>;

    static Device   rand;
    static Engine   engine{rand()};
    Distribution    dist{min, max};

    return dist(engine);
}
    
   
/*
    Channel Operation
*/
Channel_operation::Channel_operation()
    : kind{Type::none}
    , chanp{nullptr}
    , rvalp{nullptr}
    , lvalp{nullptr}
{
}


Channel_operation::Channel_operation(Channel_base* cp, const void* rvaluep)
    : kind{Type::send}
    , chanp{cp}
    , rvalp{rvaluep}
    , lvalp{nullptr}
{
    assert(cp != nullptr);
    assert(rvaluep != nullptr);
}


Channel_operation::Channel_operation(Channel_base* cp, void* lvaluep, Type optype)
    : kind{optype}
    , chanp{cp}
    , rvalp{nullptr}
    , lvalp{lvaluep}
{
    assert(cp != nullptr);
    assert(lvaluep != nullptr);
}


inline Channel_base*
Channel_operation::channel()
{
    return chanp;
}


inline const Channel_base*
Channel_operation::channel() const
{
    return chanp;
}


bool
Channel_operation::dequeue(Task::Handle task)
{
    bool is_dequeued;

    if (chanp) {
        Task::Channel_lock lock(chanp);

        switch(kind) {
        case Type::send:
            is_dequeued = chanp->dequeue_write(task, pos);
            break;

        case Type::receive:
            is_dequeued = chanp->dequeue_read(task, pos);
            break;
        }
    }

    return is_dequeued;
}


void
Channel_operation::enqueue(Task::Handle task)
{
    if (chanp) {
        switch(kind) {
        case Type::send:
            if (rvalp)
                chanp->enqueue_write(task, pos, rvalp);
            else
                chanp->enqueue_write(task, pos, lvalp);
            break;

        case Type::receive:
            chanp->enqueue_read(task, pos, lvalp);
            break;
        }
    }
}


void
Channel_operation::execute()
{
    if (chanp) {
        switch(kind) {
        case Type::send:
            if (rvalp)
                chanp->write(rvalp);
            else
                chanp->write(lvalp);
            break;

        case Type::receive:
            chanp->read(lvalp);
            break;
        }
    }
}


bool
Channel_operation::is_ready() const
{
    if (!chanp) return false;

    switch(kind) {
    case Type::send:    return chanp->is_writable();
    case Type::receive: return chanp->is_readable();
    default:            return false;
    }
}


inline void
Channel_operation::position(Channel_size n)
{
    pos = n;
}


inline Channel_size
Channel_operation::position() const
{
    return pos;
}


/*
    Scheduler Suspended Tasks
*/
inline
Scheduler::Suspended_tasks::handle_equal::handle_equal(Task::Handle task)
    : h{task}
{
}


inline bool
Scheduler::Suspended_tasks::handle_equal::operator()(const Task& task) const
{
    return task.handle() == h;
}


void
Scheduler::Suspended_tasks::insert(Task&& task)
{
    Lock lock{mutex};

    tasks.push_back(move(task));
    tasks.back().unlock();
}


Task
Scheduler::Suspended_tasks::release(Task::Handle h)
{
    Task task;
    Lock lock{mutex};
    auto taskp = find_if(tasks.begin(), tasks.end(), handle_equal(h));

    if (taskp != tasks.end()) {
        task = move(*taskp);
        tasks.erase(taskp);
    }

    return task;
}


/*
    Sheduler Task Queue
*/
void
Scheduler::Task_queue::interrupt()
{
    Lock lock{mutex};

    is_interrupt = true;
    ready.notify_one();
}


optional<Task>
Scheduler::Task_queue::pop()
{
    optional<Task>  task;
    Lock            lock{mutex};

    while (tasks.empty() && !is_interrupt)
        ready.wait(lock);

    if (!tasks.empty())
        task = pop_front(&tasks);

    return task;
}


inline Task
Scheduler::Task_queue::pop_front(std::deque<Task>* tasksp)
{
    Task task{move(tasksp->front())};

    tasksp->pop_front();
    return task;
}


void
Scheduler::Task_queue::push(Task&& task)
{
    Lock lock{mutex};

    tasks.push_back(move(task));
    ready.notify_one();
}


optional<Task>
Scheduler::Task_queue::try_pop()
{
    optional<Task>  task;
    Lock            lock{mutex, try_to_lock};

    if (lock && !tasks.empty())
        task = pop_front(&tasks);

    return task;
}


bool
Scheduler::Task_queue::try_push(Task&& task)
{
    bool is_pushed{false};
    Lock lock{mutex, try_to_lock};

    if (lock) {
        tasks.push_back(move(task));
        ready.notify_one();
        is_pushed = true;
    }

    return is_pushed;
}


/*
    Scheduler Task Queue Array
*/
Scheduler::Task_queue_array::Task_queue_array(Size n)
    : qs{n}
{
}


void
Scheduler::Task_queue_array::interrupt()
{
    for (auto& q : qs)
        q.interrupt();
}


optional<Task>
Scheduler::Task_queue_array::pop(Size qpref)
{
    const auto      nqs = qs.size();
    optional<Task>  taskp;

    // Try to dequeue a task without waiting.
    for (Size i = 0; i < nqs && !taskp; ++i) {
        auto pos = (qpref + i) % nqs;
        taskp = qs[pos].try_pop();
    }

    // If that failed, wait on the preferred queue.
    if (!taskp)
        taskp = qs[qpref].pop();

    return taskp;
}


inline void
Scheduler::Task_queue_array::push(Task&& task)
{
    const auto  nqs     = qs.size();
    const auto  qpref   = nextq++ % nqs;

    push(&qs, qpref, move(task));
}


inline void
Scheduler::Task_queue_array::push(Size qpref, Task&& task)
{
    push(&qs, qpref, move(task));
}


void
Scheduler::Task_queue_array::push(Queue_vector* qvecp, Size qpref, Task&& task)
{
    Queue_vector&   qs      = *qvecp;
    const auto      nqs     = qs.size();
    bool            done    = false;

    // Try to enqueue the task without waiting.
    for (Size i = 0; !done && i < nqs; ++i) {
        auto pos = (qpref + i) % nqs;
        if (qs[pos].try_push(move(task)))
            done = true;
    }

    // If we failed, wait on the preferred queue.
    if (!done)
        qs[qpref].push(move(task));
}


inline Scheduler::Task_queue_array::Size
Scheduler::Task_queue_array::size() const
{
    return qs.size();
}


/*
    Scheduler
*/
Scheduler::Scheduler(int nthreads)
    : workqueues{nthreads > 0 ? nthreads : Thread::hardware_concurrency()}
{
    const auto nqs = workqueues.size();

    threads.reserve(nqs);
    for (unsigned q = 0; q != nqs; ++q)
        threads.emplace_back([&,q]{ run_tasks(q); });
}


/*
    TODO:  Could this destructor should be rendered unnecessary because
    by arranging for (a) workqueues to shutdown implicitly (in their
    destructors) and (b) workqueues to be joined implicitly (in their
    destructors)?
*/
Scheduler::~Scheduler()
{
    workqueues.interrupt();
    for (auto& thread : threads)
        thread.join();
}


void
Scheduler::resume(Task::Handle task)
{
    workqueues.push(waiters.release(task));
}


/*
    TODO: The coroutine mechanism can automatically detect and store
    exceptions in the promise, so is a try-block even necessary?  Need
    to think through how exceptions thrown by tasks should be handled.
*/
void
Scheduler::run_tasks(unsigned qpos)
{
    while (optional<Task> taskp = workqueues.pop(qpos)) {
        try {
            switch(taskp->resume()) {
            case Task::Status::ready:
                workqueues.push(qpos, move(*taskp));
                break;

            case Task::Status::suspended:
                waiters.insert(move(*taskp));
                break;
            }
        } catch (...) {
            workqueues.interrupt();
        }
    }
}


void
Scheduler::submit(Task&& task)
{
    workqueues.push(move(task));
}


}   // Concurrency
}   // Isptech
