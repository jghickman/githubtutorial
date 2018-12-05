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
#include <cstdint>
#include <numeric>

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
using std::accumulate;
using std::count_if;
using std::find_if;
using std::iota;
using std::move;
using std::sort;
using std::try_to_lock;
using std::unique;
using std::upper_bound;


/*
    Data
*/
Scheduler scheduler;


/*
    Task Operation Selection Operation View
*/
inline
Task::Operation_selection::Operation_view::Operation_view(const Channel_operation* cop, Channel_size pos)
    : pop{cop}
    , index{pos}
{
}


inline Channel_base*
Task::Operation_selection::Operation_view::channel() const
{
    return pop->channel();
}


inline bool
Task::Operation_selection::Operation_view::dequeue(Task::Handle task) const
{
    return pop->dequeue(task, index);
}


inline void
Task::Operation_selection::Operation_view::enqueue(Task::Handle task) const
{
    pop->enqueue(task, index);
}


inline void
Task::Operation_selection::Operation_view::execute() const
{
    return pop->execute();
}


inline bool
Task::Operation_selection::Operation_view::is_ready() const
{
    return pop->is_ready();
}


inline Channel_size
Task::Operation_selection::Operation_view::position() const
{
    return index;
}


inline bool
Task::Operation_selection::Operation_view::operator==(Operation_view other) const
{
    return *this->pop == *other.pop;
}


inline bool
Task::Operation_selection::Operation_view::operator< (Operation_view other) const
{
    return *this->pop < *other.pop;
}


/*
    Task Operation Selection Channel Locks
*/
Task::Operation_selection::Channel_locks::Channel_locks(const Operation_vector& opers)
    : ops{opers}
{
    for_each_channel(ops, lock);
}


Task::Operation_selection::Channel_locks::~Channel_locks()
{
    for_each_channel(ops, unlock);
}


template<class T>
void
Task::Operation_selection::Channel_locks::for_each_channel(const Operation_vector& ops, T f)
{
    Channel_base* prevchanp{nullptr};

    for (const Operation_view op : ops) {
        Channel_base* chanp = op.channel();
        if (chanp && chanp != prevchanp) {
            f(chanp);
            prevchanp = chanp;
        }
    }
}


inline void
Task::Operation_selection::Channel_locks::lock(Channel_base* chanp)
{
    chanp->lock();
}


inline void
Task::Operation_selection::Channel_locks::unlock(Channel_base* chanp)
{
    chanp->unlock();
}


/*
    Task Operation Selection Unique Operations
*/
Task::Operation_selection::Transform_unique::Transform_unique(const Channel_operation* first, const Channel_operation* last, Operation_vector* outp)
{
    transform(first, last, outp);
    sort(outp->begin(), outp->end());
    remove_duplicates(outp);
}


void
Task::Operation_selection::Transform_unique::remove_duplicates(Operation_vector* vp)
{
    const auto dup = unique(vp->begin(), vp->end());
    vp->erase(dup, vp->end());
}


void
Task::Operation_selection::Transform_unique::transform(const Channel_operation* first, const Channel_operation* last, Operation_vector* outp)
{
    Operation_vector& out = *outp;

    out.resize(last - first);
    for (const Channel_operation* p = first; p != last; ++p) {
        const auto i = p - first;
        out[i] = Operation_view(p, i);
    }
        
}


/*
    Task Operation Selection
*/
Task::Select_status
Task::Operation_selection::complete(Task::Handle task, Channel_size pos)
{
    --nenqueued;
    if (!winner) {
        winner = pos;
        nenqueued -= dequeue(task, operations, pos);
    }

    return Select_status(*winner, nenqueued == 0);
}


Channel_size
Task::Operation_selection::count_ready(const Operation_vector& ops)
{
    return count_if(ops.begin(), ops.end(), [](auto op) {
        return op.is_ready();
    });
}


Channel_size
Task::Operation_selection::dequeue(Task::Handle task, const Operation_vector& ops, Channel_size selected)
{
    Channel_size n = 0;

    for (Operation_view op : ops) {
        if (op.position() != selected && op.dequeue(task))
            ++n;
    }

    return n;
}


Channel_size
Task::Operation_selection::enqueue(Task::Handle task, const Operation_vector& ops)
{
    for (Operation_view op : ops)
        op.enqueue(task);

    return ops.size();
}


Channel_size
Task::Operation_selection::pick_ready(const Operation_vector& ops, Channel_size nready)
{
    assert(ops.size() > 0 && nready > 0);

    Channel_size pos;
    Channel_size pick = random(1, nready);

    for (Channel_size i = 0, n = ops.size(); i < n; ++i) {
        if (ops[i].is_ready() && --pick == 0) {
            pos = i;
            break;
        }
    }

    return pos;
}


bool
Task::Operation_selection::select(Task::Handle task, const Channel_operation* first, const Channel_operation* last)
{
    Transform_unique    transform{first, last, &operations};
    Channel_locks       lock{operations};

    nenqueued = 0;
    winner = select_ready(operations);
    if (!winner)
        nenqueued = enqueue(task, operations);

    return nenqueued == 0;
}


optional<Channel_size>
Task::Operation_selection::select_ready(const Operation_vector& ops)
{
    optional<Channel_size> ready;

    if (ops.empty())
        ready = select_fail;
    else {
        const auto n = count_ready(ops);
        if (n > 0) {
            const auto i    = pick_ready(ops, n);
            const auto op   = ops[i];

            op.execute();
            ready = op.position();
        }
    }

    return ready;
}

   
Channel_size
Task::Operation_selection::selected() const
{
    return *winner;
}


optional<Channel_size>
Task::Operation_selection::try_select(const Channel_operation* first, const Channel_operation* last)
{
    Transform_unique    transform{first, last, &operations};
    Channel_locks       lock{operations};

    return select_ready(operations);
}


/*
    Task Future Selection Channel Locks
*/
void
Task::Future_selection::Channel_locks::acquire(const Channel_wait_vector& waits)
{
    index_unique(waits, &index);
    for (auto i : index)
        waits[i].lock_channel();
}


void
Task::Future_selection::Channel_locks::index_unique(const Channel_wait_vector& waits, Channel_index* indexp)
{
    init(indexp, waits.size());
    sort(indexp, waits);
    remove_duplicates(indexp, waits);
}


void
Task::Future_selection::Channel_locks::init(Channel_index* indexp, Channel_size n)
{
    indexp->resize(n);
    iota(indexp->begin(), indexp->end(), 0);
}


void
Task::Future_selection::Channel_locks::release(const Channel_wait_vector& waits)
{
    for (auto i : index)
        waits[i].unlock_channel();
}


void
Task::Future_selection::Channel_locks::remove_duplicates(Channel_index* indexp, const Channel_wait_vector& waits)
{
    const auto first    = indexp->begin();
    const auto last     = indexp->end();
    const auto dup      = unique(first, last, [&](auto x, auto y) {
        return waits[x].channel() == waits[y].channel();
    });

    indexp->erase(dup, last);
}


void
Task::Future_selection::Channel_locks::sort(Channel_index* indexp, const Channel_wait_vector& waits)
{
    std::sort(indexp->begin(), indexp->end(), [&](auto x, auto y) {
        return waits[x].channel() < waits[y].channel();
    });
}


/*
    Task Future Selection Wait Set
*/
Channel_size
Task::Future_selection::Wait_set::complete_readable(Task::Handle task, Channel_size chan)
{
    const Channel_size future = channels[chan].future();

    futures[future].complete(task, channels, chan);
    --nenqueued;
    return future;
}


Channel_size
Task::Future_selection::Wait_set::count_ready(const Future_wait_index& index, const Future_wait_vector& futures, const Channel_wait_vector& chans)
{
    return count_if(index.begin(), index.end(), [&](auto i) {
        return futures[i].is_ready(chans);
    });
}


void
Task::Future_selection::Wait_set::dequeue_all(Task::Handle task)
{
    for (auto i : futureindex)
        futures[i].dequeue_locked(task, channels);

    nenqueued = 0;
}


void
Task::Future_selection::Wait_set::dequeue_not_ready(Task::Handle task)
{
    if (nenqueued > 0) {
        nenqueued -= accumulate(futureindex.begin(), futureindex.end(), 0, [&](auto accum, auto i) {
            const auto& f = futures[i];
            if (!f.is_ready(channels)) {
                f.dequeue(task, channels);
                ++accum;
            }
            return accum;
        });
    }
}


void
Task::Future_selection::Wait_set::end_setup()
{
    locks.release(channels);
}


void
Task::Future_selection::Wait_set::enqueue_all(Task::Handle task)
{
    for (auto i : futureindex)
        futures[i].enqueue(task, channels);

    nenqueued = futureindex.size();
}


Channel_size
Task::Future_selection::Wait_set::enqueue_not_ready(Task::Handle task)
{
    const auto n = accumulate(futureindex.begin(), futureindex.end(), 0, [&](auto accum, auto i) {
        const auto& f = futures[i];
        if (!f.is_ready(channels)) {
            f.enqueue(task, channels);
            ++accum;
        }
        return accum;
    });

    nenqueued += n;
    return n;
}


void
Task::Future_selection::Wait_set::index_unique(const Future_wait_vector& waits, Future_wait_index* indexp)
{
    init(indexp, waits.size());
    sort(indexp, waits);
    remove_duplicates(indexp, waits);
}


void
Task::Future_selection::Wait_set::init(Future_wait_index* indexp, Channel_size n)
{
    indexp->resize(n);
    iota(indexp->begin(), indexp->end(), 0);
}


optional<Channel_size>
Task::Future_selection::Wait_set::pick_ready(const Future_wait_index& index, const Future_wait_vector& futures, const Channel_wait_vector& chans, Channel_size nready)
{
    optional<Channel_size> ready;

    if (nready > 0) {
        Channel_size choice = random(1, nready);
        for (auto i : index) {
            if (futures[i].is_ready(chans) && --choice == 0) {
                ready = i;
                break;
            }
        }
    }

    return ready;
}


void
Task::Future_selection::Wait_set::remove_duplicates(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    const auto dup = unique(indexp->begin(), indexp->end(), [&](auto x, auto y) {
        return waits[x] == waits[y];
    });

    indexp->erase(dup, indexp->end());
}


optional<Channel_size>
Task::Future_selection::Wait_set::select_ready()
{
    const auto n = count_ready(futureindex, futures, channels);
    return pick_ready(futureindex, futures, channels, n);
}


void
Task::Future_selection::Wait_set::sort(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    std::sort(indexp->begin(), indexp->end(), [&](auto x, auto y) {
        return waits[x] < waits[y];
    });
}


/*
    Task Future Selection Timer
*/
inline void
Task::Future_selection::Timer::cancel(Task::Handle task) const
{
    scheduler.cancel_wait_timer(task);
    state = State::cancel_pending;
}


inline void
Task::Future_selection::Timer::complete_cancel() const
{
    state = State::cancel_complete;
}


inline void
Task::Future_selection::Timer::expire(Time_point /*when*/) const
{
    if (state == State::cancel_pending)
        state = State::cancel_complete;
    else
        state = State::expired;
}


inline bool
Task::Future_selection::Timer::is_active() const
{
    switch(state) {
    case State::running:
    case State::cancel_pending:
        return true;

    default:
        return false;
    }
}


inline bool
Task::Future_selection::Timer::is_cancelled() const
{
    switch(state) {
    case State::cancel_pending:
    case State::cancel_complete:
        return true;

    default:
        return false;
    }
}


inline bool
Task::Future_selection::Timer::is_running() const
{
    return state == State::running;
}


/*
    Task Future Selection
*/
bool
Task::Future_selection::cancel_timer()
{
    timer.complete_cancel();
    return !waits.enqueued();
}


Task::Select_status
Task::Future_selection::complete_readable(Task::Handle task, Channel_size chan)
{
    const Channel_size future = waits.complete_readable(task, chan);

    if (!waits.enqueued() && timer.is_running())
        timer.cancel(task);

    if (!result) {
        result = future;
        if (waittype == Wait_type::any)
            waits.dequeue_not_ready(task);
    }

    return Select_status(*result, is_ready(waits, timer));
}


bool
Task::Future_selection::complete_timer(Task::Handle task, Time_point time)
{
    timer.expire(time);
    if (!timer.is_cancelled()) {
        result = wait_fail;
        waits.dequeue_not_ready(task);
    }

    return !waits.enqueued();
}


inline bool
Task::Future_selection::is_ready(const Wait_set& waits, Timer timer)
{
    return !(waits.enqueued() || timer.is_active());
}



/*
    Task Promise
*/
Task::Promise::Promise()
    : taskstat{Status::ready}
{
}


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
Channel_operation::channel() const
{
    return chanp;
}


bool
Channel_operation::dequeue(Task::Handle task, Channel_size pos) const
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
Channel_operation::enqueue(Task::Handle task, Channel_size pos) const
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
Channel_operation::execute() const
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
    Task        task;
    Lock        lock{mutex};
    const auto  taskp = find_if(tasks.begin(), tasks.end(), handle_equal(h));

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


Task
Scheduler::Task_queue::pop()
{
    Task task;
    Lock lock{mutex};

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


Task
Scheduler::Task_queue::try_pop()
{
    Task task;
    Lock lock{mutex, try_to_lock};

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


Task
Scheduler::Task_queue_array::pop(Size qpref)
{
    const auto  nqs = qs.size();
    Task        task;

    // Try to dequeue a task without waiting.
    for (Size i = 0; i < nqs && !task; ++i) {
        auto pos = (qpref + i) % nqs;
        task = qs[pos].try_pop();
    }

    // If that failed, wait on the preferred queue.
    if (!task)
        task = qs[qpref].pop();

    return task;
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
    Scheduler Timers Request
*/
inline
Scheduler::Timers::Request::Request(Task::Handle h)
    : taskh{h}
    , duration{}
{
}


inline
Scheduler::Timers::Request::Request(Task::Handle h, nanoseconds t)
    : taskh{h}
    , duration{t}
{
}


inline bool
Scheduler::Timers::Request::is_cancel() const
{
    using namespace std::literals::chrono_literals;

    return duration == 0ns;
}


inline bool
Scheduler::Timers::Request::is_start() const
{
    return !is_cancel();
}


inline Task::Handle
Scheduler::Timers::Request::task() const
{
    return taskh;
}


inline nanoseconds
Scheduler::Timers::Request::time() const
{
    return duration;
}


/*
    Scheduler Timers Alarm Queue
*/
inline Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::begin() const
{
    return alarms.begin();
}


inline Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::end() const
{
    return alarms.end();
}


inline void
Scheduler::Timers::Alarm_queue::erase(Iterator p)
{
    alarms.erase(p);
}


Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::find(Task::Handle task) const
{
    const auto first    = alarms.begin();
    const auto last     = alarms.end();
    const auto p        = find_if(first, last, task_eq(task));

    return (p != last && p->task == task) ? p : last;
}


inline const Scheduler::Timers::Alarm&
Scheduler::Timers::Alarm_queue::front() const
{
    return alarms.front();
}


inline bool
Scheduler::Timers::Alarm_queue::is_empty() const
{
    return alarms.empty();
}


inline Scheduler::Timers::Alarm
Scheduler::Timers::Alarm_queue::pop()
{
    Alarm a = alarms.front();

    alarms.erase(alarms.begin());
    return a;
}


void
Scheduler::Timers::Alarm_queue::push(const Alarm& a)
{
    const auto p = upper_bound(alarms.begin(), alarms.end(), a);
    alarms.insert(p, a);
}


int
Scheduler::Timers::Alarm_queue::size() const
{
    return static_cast<int>(alarms.size());
}


/*
    Scheduler Timers Windows Handles
*/
Scheduler::Timers::Windows_handles::Windows_handles()
{
    int n = 0;

    try {
        assert(request_pos == n);
        hs[request_pos] = CreateEvent(NULL, FALSE, FALSE, NULL);
        ++n;

        assert(timer_pos == n);
        hs[timer_pos] = CreateWaitableTimer(NULL, FALSE, NULL);
        ++n;

        assert(interrupt_pos == n);
        hs[interrupt_pos] = CreateEvent(NULL, FALSE, FALSE, NULL);
        ++n;
    }
    catch (...) {
        close(hs, n);
    }
}


inline
Scheduler::Timers::Windows_handles::~Windows_handles()
{
    close(hs);
}


void
Scheduler::Timers::Windows_handles::close(Windows_handle* hs, int n)
{
    for (int i = 0; i < n; ++i)
        CloseHandle(hs[n - i - 1]);
}


inline void
Scheduler::Timers::Windows_handles::signal_interrupt() const
{
    SetEvent(hs[interrupt_pos]);
}


inline void
Scheduler::Timers::Windows_handles::signal_request() const
{
    SetEvent(hs[request_pos]);
}


inline Scheduler::Timers::Windows_handle
Scheduler::Timers::Windows_handles::timer() const
{
    return hs[timer_pos];
}


inline int
Scheduler::Timers::Windows_handles::wait_any(Lock* lockp) const 
{
    DWORD n;

    lockp->unlock();
    n = WaitForMultipleObjects(size, hs, FALSE, INFINITE);
    lockp->lock();

    return static_cast<int>(n - WAIT_OBJECT_0);
}


/*
    Scheduler Timers
*/
Scheduler::Timers::Timers()
    : thread{[&]() { run_thread(); }}
{
}


Scheduler::Timers::~Timers()
{
    handles.signal_interrupt();
    thread.join();
}


void
Scheduler::Timers::activate_alarm(Windows_handle timer, const Request& request, Alarm_queue* queuep, Time_point now)
{
    const Alarm alarm = make_alarm(request, now);

    queuep->push(alarm);
    if (alarm == queuep->front())
        set_timer(timer, alarm, now);
}


void
Scheduler::Timers::cancel_wait(Task::Handle task)
{
    const Lock lock{mutex}; 

    requestq.push(make_cancel(task));
    handles.signal_request();
}


void
Scheduler::Timers::cancel_alarm(Windows_handle timer, const Request& request, Alarm_queue* queuep, Time_point now)
{
    const auto alarmp = queuep->find(request.task());

    if (alarmp != queuep->end()) {
        notify_cancel(*alarmp);
        remove_alarm(timer, queuep, alarmp, now);
    }
}


inline void
Scheduler::Timers::cancel_timer(Windows_handle handle)
{
    CancelWaitableTimer(handle);
}


inline bool
Scheduler::Timers::is_expired(const Alarm& alarm, Time_point now)
{
    return alarm.expiry <= now;
}


inline Scheduler::Timers::Alarm
Scheduler::Timers::make_alarm(const Request& request, Time_point now)
{
    return Alarm(request.task(), now + request.time());
}


inline Scheduler::Timers::Request
Scheduler::Timers::make_cancel(Task::Handle task)
{
    return Request(task);
}


inline Scheduler::Timers::Request
Scheduler::Timers::make_start(Task::Handle task, nanoseconds duration)
{
    return Request(task, duration);
}


inline void
Scheduler::Timers::notify_cancel(const Alarm& alarm)
{
    Task::Handle task = alarm.task;

    if (task.promise().cancel_wait_timer())
        scheduler.resume(task);
}


void
Scheduler::Timers::notify_complete(const Alarm& alarm, Time_point now, Lock* lockp)
{
    Task::Handle task = alarm.task;

    /*
        The task could be simultaneously attempting to cancel the timer,
        so temporarily release the timer lock to avoid a deadly embrace
        between the task and the timer thread.
    */
    lockp->unlock();
    if (task.promise().complete_wait_timer(now))
        scheduler.resume(task);
    lockp->lock();
}


void
Scheduler::Timers::process_alarms(Windows_handle timer, Alarm_queue* queuep, Lock* lockp)
{
    const Time_point now = Clock::now();

    while (!queuep->is_empty() && is_expired(queuep->front(), now))
        notify_complete(queuep->pop(), now, lockp);

    if (!queuep->is_empty())
        set_timer(timer, queuep->front(), now);
}


void
Scheduler::Timers::process_request(Windows_handle timer, const Request& request, Alarm_queue* queuep, Time_point now)
{
    if (request.is_start())
        activate_alarm(timer, request, queuep, now);
    else if (request.is_cancel())
        cancel_alarm(timer, request, queuep, now);
}


void
Scheduler::Timers::process_requests(Windows_handle timer, Request_queue* requestqp, Alarm_queue* queuep)
{
    const Time_point now = Clock::now();

    while (!requestqp->empty()) {
        process_request(timer, requestqp->front(), queuep, now);
        requestqp->pop();
    }
}


void
Scheduler::Timers::remove_alarm(Windows_handle timer, Alarm_queue* queuep, Alarm_queue::Iterator p, Time_point now)
{
    // If the alarm is the next to fire, update the timer.
    if (p == queuep->begin()) {
        const auto nextp = std::next(p);
        if (nextp == queuep->end())
            cancel_timer(timer);
        else if (*p < *nextp)
            set_timer(timer, *nextp, now);
    }

    queuep->erase(p);
}


void
Scheduler::Timers::run_thread()
{
    bool done{false};
    Lock lock{mutex};

    while (!done) {
        switch(handles.wait_any(&lock)) {
        case Windows_handles::request_pos:
            process_requests(handles.timer(), &requestq, &alarmq);
            break;

        case Windows_handles::timer_pos:
            process_alarms(handles.timer(), &alarmq, &lock);
            break;

        default:
            done = true;
            break;
        }
    }
}


void
Scheduler::Timers::set_timer(Windows_handle timer, const Alarm& alarm, Time_point now)
{
    static const int nanosecs_per_tick = 100;

    const nanoseconds   dt      =  alarm.expiry - now;
    const std::int64_t  timerdt = -(dt.count() / nanosecs_per_tick);
    LARGE_INTEGER       timerbuf;

    timerbuf.LowPart = static_cast<DWORD>(timerdt & 0xFFFFFFFF);
    timerbuf.HighPart = static_cast<LONG>(timerdt >> 32);
    SetWaitableTimer(timer, &timerbuf, 0, NULL, NULL, FALSE);
}


void
Scheduler::Timers::start_wait(Task::Handle task, nanoseconds duration)
{
    const Lock lock{mutex};

    requestq.push(make_start(task, duration));
    handles.signal_request();
}


/*
    Scheduler
*/
Scheduler::Scheduler(int nthreads)
    : ready{nthreads > 0 ? nthreads : Thread::hardware_concurrency()}
{
    const auto nqs = ready.size();

    processors.reserve(nqs);
    for (unsigned q = 0; q != nqs; ++q)
        processors.emplace_back([&,q]{ run_tasks(q); });
}


/*
    TODO:  Could this destructor should be rendered unnecessary by
    arranging for (a) workqueues to shutdown implicitly (in their
    destructors) and (b) workqueues to be joined implicitly (in their
    destructors)?
*/
Scheduler::~Scheduler()
{
    ready.interrupt();
    for (auto& proc : processors)
        proc.join();
}


void
Scheduler::cancel_wait_timer(Task::Handle task)
{
    timers.cancel_wait(task);
}


void
Scheduler::resume(Task::Handle task)
{
    ready.push(suspended.release(task));
}


void
Scheduler::run_tasks(unsigned qpos)
{
    while (Task task = ready.pop(qpos)) {
        try {
            switch(task.resume()) {
            case Task::Status::ready:
                ready.push(qpos, move(task));
                break;

            case Task::Status::suspended:
                suspended.insert(move(task));
                break;
            }
        } catch (...) {
            ready.interrupt();
        }
    }
}


void
Scheduler::start_wait_timer(Task::Handle task, nanoseconds duration)
{
    timers.start_wait(task, duration);
}


void
Scheduler::submit(Task&& task)
{
    ready.push(move(task));
}


}   // Concurrency
}   // Isptech
