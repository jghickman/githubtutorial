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
using std::move;
using std::sort;
using std::try_to_lock;
using std::upper_bound;


/*
    Data
*/
Scheduler scheduler;


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


/*
    Task Channel Selection
*/
Channel_size
Task::Channel_selection::count_ready(const Channel_operation* first, const Channel_operation* last)
{
    return count_if(first, last, [](const auto& co) {
        return co.is_ready();
    });
}


Channel_size
Task::Channel_selection::dequeue(Task::Handle task, Channel_operation* first, Channel_operation* last, Channel_size selected)
{
    Channel_size n = 0;

    for (Channel_operation* cop = first; cop != last; ++cop) {
        if (cop->position() != selected && cop->dequeue(task))
            ++n;
    }

    return n;
}


Channel_size
Task::Channel_selection::enqueue(Task::Handle task, Channel_operation* first, Channel_operation* last)
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
    sort(first, last, [](const auto& x, const auto& y) {
        return x.position() < y.position();
    });
}


void
Task::Channel_selection::save_positions(Channel_operation* first, Channel_operation* last)
{
    for (Channel_operation* cop = first; cop != last; ++cop)
        cop->position(cop - first);
}


Task::Select_status
Task::Channel_selection::select(Task::Handle task, Channel_size pos)
{
    --nenqueued;
    if (!winner) {
        winner = pos;
        nenqueued -= dequeue(task, begin, end, pos);
    }

    return Select_status(*winner, nenqueued == 0);
}


bool
Task::Channel_selection::select(Task::Handle task, Channel_operation* first, Channel_operation* last)
{
    Sort_guard chansort{first, last};
    Lock_guard chanlocks{first, last};

    nenqueued   = 0;
    winner      = select_ready(first, last);

    // If nothing is ready, enqueue the operations.
    if (!winner) {
        nenqueued = enqueue(task, first, last);
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
    sort(first, last, [](const auto& x, const auto& y) {
        return x.channel() < y.channel();
    });
}


optional<Channel_size>
Task::Channel_selection::try_select(Channel_operation* first, Channel_operation* last)
{
    Sort_guard chansort{first, last};
    Lock_guard chanlocks{first, last};

    return select_ready(first, last);
}


/*
    Task Future Selection Wait Set
*/
Channel_size
Task::Future_selection::Wait_set::complete_channel(Task::Handle task, Channel_size pos)
{
    const Channel_size fpos = channels[pos].future();

    futures[fpos].complete(task, channels, pos);
    --nenqueued;
    return fpos;
}


Channel_size
Task::Future_selection::Wait_set::count_ready(const Future_wait_vector& fs, const Channel_wait_vector& chans)
{
    return count_if(fs.begin(), fs.end(), [&](const auto& future) {
        return future.is_ready(chans);
    });
}


void
Task::Future_selection::Wait_set::dequeue_all(Task::Handle task)
{
    for (const auto& f : futures)
        f.dequeue_locked(task, channels);

    nenqueued = 0;
}


void
Task::Future_selection::Wait_set::dequeue_not_ready(Task::Handle task)
{
    if (nenqueued > 0) {
        nenqueued -= accumulate(futures.begin(), futures.end(), 0, [&](auto n, const auto& f) {
            if (!f.is_ready(channels)) {
                f.dequeue(task, channels);
                ++n;
            }
            return n;
        });
    }
}


void
Task::Future_selection::Wait_set::enqueue_all(Task::Handle task)
{
    for (const auto& f : futures)
        f.enqueue(task, channels);

    nenqueued = futures.size();
}


Channel_size
Task::Future_selection::Wait_set::enqueue_not_ready(Task::Handle task)
{
    const auto n = accumulate(futures.begin(), futures.end(), 0, [&](auto n, const auto& f) {
        if (!f.is_ready(channels)) {
            f.enqueue(task, channels);
            ++n;
        }
        return n;
    });

    nenqueued += n;
    return n;
}


void
Task::Future_selection::Wait_set::lock(Channel_wait_vector* waitsp)
{
    for (const auto& wait : *waitsp)
        wait.lock_channel();
}


optional<Channel_size>
Task::Future_selection::Wait_set::pick_ready(const Future_wait_vector& futures, const Channel_wait_vector& chans, Channel_size nready)
{
    using Size = Future_wait_vector::size_type;

    optional<Channel_size> ready;

    if (nready > 0) {
        Channel_size choice = random(1, nready);
        for (Size i = 0, n = futures.size(); i < n; ++i) {
            if (futures[i].is_ready(chans) && --choice == 0) {
                ready = i;
                break;
            }
        }
    }

    return ready;
}


optional<Channel_size>
Task::Future_selection::Wait_set::select_ready()
{
    const auto n = count_ready(futures, channels);
    return pick_ready(futures, channels, n);
}


void
Task::Future_selection::Wait_set::sort(Channel_wait_vector* waitsp)
{
    return std::sort(waitsp->begin(), waitsp->end(), [](const auto& x, const auto& y) {
        return x.channel() < y.channel();
    });
}


void
Task::Future_selection::Wait_set::unlock(Channel_wait_vector* waitsp)
{
    for (const auto& wait : *waitsp)
        wait.unlock_channel();
}


/*
    Task Future Selection Timer
*/
inline void
Task::Future_selection::Timer::cancel(Task::Handle task) const
{
    scheduler.cancel_timer(task);
    state = State::cancel_pending;
}


inline void
Task::Future_selection::Timer::complete(Time_point /*when*/) const
{
    if (state == State::cancel_pending)
        state = State::cancel_complete;
    else
        state = State::complete;
}


inline void
Task::Future_selection::Timer::complete_cancel() const
{
    state = State::cancel_complete;
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


/*
    Task Future Selection
*/
bool
Task::Future_selection::cancel_timer()
{
    timer.complete_cancel();
    return waits.enqueued() == 0;
}


bool
Task::Future_selection::complete_timer(Task::Handle task, Time_point time)
{
    timer.complete(time);
    if (!timer.is_cancelled()) {
        result = wait_fail;
        waits.dequeue_not_ready(task);
    }

    return waits.enqueued() == 0;
}


Task::Select_status
Task::Future_selection::select_channel(Task::Handle task, Channel_size pos)
{
    const Channel_size future = waits.complete_channel(task, pos);

    if (waits.enqueued() == 0 && timer.is_active() && !timer.is_cancelled())
        timer.cancel(task);

    if (!result) {
        result = future;
        if (type == Type::any)
            waits.dequeue_not_ready(task);
    }

    return Select_status(*result, waits.enqueued() == 0 && !timer.is_active());
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


inline Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::find(Task::Handle task) const
{
    const auto p = find_if(alarms.begin(), alarms.end(), task_eq(task));
    return (p->task == task) ? p : alarms.end();
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
Scheduler::Timers::cancel(Task::Handle task)
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

    if (task.promise().cancel_timer())
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
    if (task.promise().complete_timer(now))
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
Scheduler::Timers::start(Task::Handle task, nanoseconds duration)
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
Scheduler::cancel_timer(Task::Handle task)
{
    timers.cancel(task);
}


void
Scheduler::resume(Task::Handle task)
{
    ready.push(suspended.release(task));
}


void
Scheduler::run_tasks(unsigned qpos)
{
    while (optional<Task> taskp = ready.pop(qpos)) {
        try {
            switch(taskp->resume()) {
            case Task::Status::ready:
                ready.push(qpos, move(*taskp));
                break;

            case Task::Status::suspended:
                suspended.insert(move(*taskp));
                break;
            }
        } catch (...) {
            ready.interrupt();
        }
    }
}


void
Scheduler::start_timer(Task::Handle task, nanoseconds duration)
{
    timers.start(task, duration);
}


void
Scheduler::submit(Task&& task)
{
    ready.push(move(task));
}


}   // Concurrency
}   // Isptech
