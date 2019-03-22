//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/coroutine/task.cpp
//

//
//  IAPPA CM Revision # : $Revision: 1.8 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2019/01/18 23:06:57 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/src/isptech/coroutine/task.cpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


#include "isptech/coroutine/task.hpp"
#include <cstdint>
#include <iostream>
#include <numeric>

#pragma warning(disable: 4073)
#pragma init_seg(lib)


/*
    Information and Sensor Processing Technology Coroutine Library
*/
namespace Isptech   {
namespace Coroutine {


/*
    Names/Types
*/
using std::accumulate;
using std::count_if;
using std::find_if;
using std::iota;
using std::move;
using std::next;
using std::literals::chrono_literals::operator""ns;
using std::sort;
using std::try_to_lock;
using std::unique;
using std::upper_bound;


/*
    Data
*/
Scheduler scheduler;


/*
    Task Operation Selector Operation View
*/
inline
Task::Operation_selector::Operation_view::Operation_view(const Channel_operation* cop, Channel_size pos)
    : pop{cop}
    , index{pos}
{
}


inline Channel_base*
Task::Operation_selector::Operation_view::channel() const
{
    return pop->channel();
}


inline bool
Task::Operation_selector::Operation_view::dequeue(Task::Promise* taskp) const
{
    return pop->dequeue(taskp, index);
}


inline void
Task::Operation_selector::Operation_view::enqueue(Task::Promise* taskp) const
{
    pop->enqueue(taskp, index);
}


inline void
Task::Operation_selector::Operation_view::execute() const
{
    return pop->execute();
}


inline bool
Task::Operation_selector::Operation_view::is_ready() const
{
    return pop->is_ready();
}


inline Channel_size
Task::Operation_selector::Operation_view::position() const
{
    return index;
}


inline bool
Task::Operation_selector::Operation_view::operator==(Operation_view other) const
{
    return *this->pop == *other.pop;
}


inline bool
Task::Operation_selector::Operation_view::operator< (Operation_view other) const
{
    return *this->pop < *other.pop;
}


/*
    Task Operation Selector Channel Locks
*/
inline
Task::Operation_selector::Channel_locks::Channel_locks(const Operation_vector& opers)
    : ops{opers}
{
    for_each_channel(ops, lock);
}


inline
Task::Operation_selector::Channel_locks::~Channel_locks()
{
    for_each_channel(ops, unlock);
}


template<class T>
void
Task::Operation_selector::Channel_locks::for_each_channel(const Operation_vector& ops, T f)
{
    Channel_base* prevchanp{nullptr};

    for (const auto op : ops) {
        Channel_base* chanp = op.channel();
        if (chanp && chanp != prevchanp) {
            f(chanp);
            prevchanp = chanp;
        }
    }
}


inline void
Task::Operation_selector::Channel_locks::lock(Channel_base* chanp)
{
    chanp->lock();
}


inline void
Task::Operation_selector::Channel_locks::unlock(Channel_base* chanp)
{
    chanp->unlock();
}


/*
    Task Operation Selector Unique Operations
*/
Task::Operation_selector::Transform_unique::Transform_unique(const Channel_operation* first, const Channel_operation* last, Operation_vector* outp)
{
    /*
        TODO: Could writing loops to skip duplicate operations (rather than
        adding a step to remove them) result in significantly better
        performance due to improved algorithmic efficiency?
    */
    transform(first, last, outp);
    sort(outp->begin(), outp->end());
    remove_duplicates(outp);
}


void
Task::Operation_selector::Transform_unique::remove_duplicates(Operation_vector* vp)
{
    const auto dup = unique(vp->begin(), vp->end());
    vp->erase(dup, vp->end());
}


void
Task::Operation_selector::Transform_unique::transform(const Channel_operation* first, const Channel_operation* last, Operation_vector* outp)
{
    Operation_vector& out = *outp;

    out.resize(last - first);
    for (const Channel_operation* cop = first; cop != last; ++cop) {
        const auto i = cop - first;
        out[i] = {cop, i};
    }
        
}


/*
    Task Operation Selector
*/
Channel_size
Task::Operation_selector::count_ready(const Operation_vector& ops)
{
    return count_if(ops.begin(), ops.end(), [](const auto op) {
        return op.is_ready();
    });
}


Channel_size
Task::Operation_selector::dequeue(Task::Promise* taskp, const Operation_vector& ops, Channel_size selected)
{
    Channel_size n = 0;

    for (const auto op : ops) {
        if (op.position() != selected && op.dequeue(taskp))
            ++n;
    }

    return n;
}


Channel_size
Task::Operation_selector::enqueue(Task::Promise* taskp, const Operation_vector& ops)
{
    for (const auto op : ops)
        op.enqueue(taskp);

    return ops.size();
}


Channel_size
Task::Operation_selector::get_ready(const Operation_vector& ops, Channel_size n)
{
    assert(n > 0);

    Channel_size ready = n;

    for (Channel_size i = 0, remaining = n; ready == n; ++i) {
        if (ops[i].is_ready() && --remaining == 0)
            ready = i;
    }

    return ready;
}


Task::Select_status
Task::Operation_selector::notify_complete(Task::Promise* taskp, Channel_size pos)
{
    --nenqueued;
    if (!winner) {
        winner = pos;
        nenqueued -= dequeue(taskp, operations, pos);
    }

    return Select_status(*winner, nenqueued == 0);
}


Channel_size
Task::Operation_selector::pick_ready(const Operation_vector& ops, Channel_size nready)
{
    assert(ops.size() > 0 && nready > 0);

    const Channel_size choice = random(1, nready);
    return get_ready(ops, choice);
}


bool
Task::Operation_selector::select(Task::Promise* taskp, const Channel_operation* first, const Channel_operation* last)
{
    const Transform_unique  transform{first, last, &operations};
    const Channel_locks     lock{operations};

    winner = select_ready(operations);
    nenqueued = winner ? 0 : enqueue(taskp, operations);
    return !nenqueued;
}


optional<Channel_size>
Task::Operation_selector::select_ready(const Operation_vector& ops)
{
    optional<Channel_size> ready;

    if (const auto n = count_ready(ops)) {
        const auto i    = pick_ready(ops, n);
        const auto op   = ops[i];

        op.execute();
        ready = op.position();
    }

    return ready;
}

   
Channel_size
Task::Operation_selector::selected() const
{
    return *winner;
}


optional<Channel_size>
Task::Operation_selector::try_select(const Channel_operation* first, const Channel_operation* last)
{
    const Transform_unique  transform{first, last, &operations};
    const Channel_locks     lock{operations};

    return select_ready(operations);
}


/*
    Task Future Selector Channel Locks
*/
void
Task::Future_selector::Channel_locks::acquire(const Future_wait_vector& fws, const Channel_wait_vector& cws, const Future_wait_index& index)
{
    transform(fws, cws, index, &channels);
    sort(&channels);
    lock(channels);
}


template<class T>
void
Task::Future_selector::Channel_locks::for_each_unique(const Channel_vector& cs, T func)
{
    Channel_base* prev = nullptr;

    for (auto pp = cs.begin(), end = cs.end(); pp != end; ++pp) {
        Channel_base* p = *pp;
        if (p && p != prev)
            func(p);
        prev = p;
    }
}


inline void
Task::Future_selector::Channel_locks::lock(const Channel_vector& chans)
{
    for_each_unique(chans, lock_channel);
}


inline void
Task::Future_selector::Channel_locks::lock_channel(Channel_base* chanp)
{
    chanp->lock();
}


inline
Task::Future_selector::Channel_locks::operator bool() const
{
    return !channels.empty();
}


void
Task::Future_selector::Channel_locks::release()
{
    unlock(channels);
    channels.clear();
}


inline void
Task::Future_selector::Channel_locks::sort(Channel_vector* chansp)
{
    std::sort(chansp->begin(), chansp->end());
}


void
Task::Future_selector::Channel_locks::transform(const Future_wait_vector& fws, const Channel_wait_vector& cws, const Future_wait_index& index, Channel_vector* chansp)
{
    chansp->reserve(cws.size());

    for (auto i : index) {
        const auto& fw = fws[i];
        chansp->push_back(cws[fw.value()].channel());
        chansp->push_back(cws[fw.error()].channel());
    }
}


inline void
Task::Future_selector::Channel_locks::unlock(const Channel_vector& chans)
{
    for_each_unique(chans, unlock_channel);
}


inline void
Task::Future_selector::Channel_locks::unlock_channel(Channel_base* chanp)
{
    chanp->unlock();
}


/*
    Task Future Selector Future Wait
*/
bool
Task::Future_selector::Future_wait::complete(Task::Promise* taskp, const Channel_wait_vector& waits, Channel_size pos) const
{
    const Channel_wait& wait        = waits[pos];
    const auto          otherpos    = (pos == vpos) ? epos : vpos;
    const Channel_wait& other       = waits[otherpos];

    *signalp = true;
    wait.complete();
    dequeue_unlocked(taskp, other, otherpos);
    return !other.is_enqueued();
}


bool
Task::Future_selector::Future_wait::dequeue(Task::Promise* taskp, const Channel_wait_vector& waits) const
{
    const auto& v           = waits[vpos];
    const auto& e           = waits[epos];
    const bool was_enqueued = is_enqueued(v, e);

    v.dequeue(taskp, vpos);
    e.dequeue(taskp, epos);

    return was_enqueued && !is_enqueued(v, e);
}


inline void
Task::Future_selector::Future_wait::dequeue_unlocked(Task::Promise* taskp, const Channel_wait& wait, Channel_size pos)
{
    const Channel_lock lock{wait.channel()};
    wait.dequeue(taskp, pos);
}


bool
Task::Future_selector::Future_wait::dequeue_unlocked(Task::Promise* taskp, const Channel_wait_vector& waits) const
{
    const auto& v           = waits[vpos];
    const auto& e           = waits[epos];
    const bool was_enqueued = is_enqueued(v, e);

    dequeue_unlocked(taskp, v, vpos);
    dequeue_unlocked(taskp, e, epos);

    return was_enqueued && !is_enqueued(v, e);
}


inline bool
Task::Future_selector::Future_wait::is_enqueued(const Channel_wait& x, const Channel_wait& y)
{
    return x.is_enqueued() || y.is_enqueued();
}


/*
    Task Future Selector Dequeue Locked
*/
inline
Task::Future_selector::dequeue_locked::dequeue_locked(Task::Promise* tp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
    : taskp{tp}
    , futures{fs}
    , channels{cs}
{
}


Channel_size
Task::Future_selector::dequeue_locked::operator()(Channel_size n, Channel_size i) const
{
    if (futures[i].dequeue(taskp, channels))
        ++n;

    return n;
}


/*
    Task Future Selector Dequeue Unlocked
*/
inline
Task::Future_selector::dequeue_unlocked::dequeue_unlocked(Task::Promise* tp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
    : taskp{tp}
    , futures{fs}
    , channels{cs}
{
}


Channel_size
Task::Future_selector::dequeue_unlocked::operator()(Channel_size n, Channel_size i) const
{
    if (futures[i].dequeue_unlocked(taskp, channels))
        ++n;

    return n;
}


/*
    Task Future Selector Enqueue Not Ready
*/
inline
Task::Future_selector::enqueue_not_ready::enqueue_not_ready(Task::Promise* tp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
    : taskp{tp}
    , futures{fs}
    , channels{cs}
{
}


Channel_size
Task::Future_selector::enqueue_not_ready::operator()(Channel_size n, Channel_size i) const
{
    const auto& f = futures[i];

    if (!f.is_ready(channels)) {
        f.enqueue(taskp, channels);
        ++n;
    }

    return n;
}


/*
    Task Future Selector Future Set
*/
Channel_size
Task::Future_selector::Future_set::count_ready(const Future_wait_index& index, const Future_wait_vector& fs, const Channel_wait_vector& cs)
{
    return count_if(index.begin(), index.end(), [&](auto i) {
        return fs[i].is_ready(cs);
    });
}


Channel_size
Task::Future_selector::Future_set::dequeue(Task::Promise* taskp)
{
    Channel_size n = 0;

    if (nenqueued > 0) {
        if (locks)
            n = accumulate(index.begin(), index.end(), n, dequeue_locked(taskp, futures, channels));
        else
            n = accumulate(index.begin(), index.end(), n, dequeue_unlocked(taskp, futures, channels));

        nenqueued -= n;
    }

    return n;
}


Channel_size
Task::Future_selector::Future_set::enqueue(Task::Promise* taskp)
{
    Channel_size n = 0;

    n = accumulate(index.begin(), index.end(), n, enqueue_not_ready(taskp, futures, channels));
    nenqueued += n;
    return n;
}


Channel_size
Task::Future_selector::Future_set::get_ready(const Future_wait_index& index, const Future_wait_vector& futures, const Channel_wait_vector& chans, Channel_size n)
{
    assert(n > 0);

    Channel_size ready = n;

    for (Channel_size i = 0, remaining = n; ready == n; ++i) {
        if (futures[index[i]].is_ready(chans) && --remaining == 0)
            ready = i;
    }

    return ready;
}


void
Task::Future_selector::Future_set::index_unique(const Future_wait_vector& waits, Future_wait_index* indexp)
{
    init(indexp, waits);
    sort(indexp, waits);
    remove_duplicates(indexp, waits);
}


void
Task::Future_selector::Future_set::init(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    const auto n = waits.size();

    indexp->resize(n);
    iota(indexp->begin(), indexp->end(), 0);
}


void
Task::Future_selector::Future_set::lock_channels()
{
    locks.acquire(futures, channels, index);
}


Channel_size
Task::Future_selector::Future_set::notify_readable(Task::Promise* taskp, Channel_size chan)
{
    const Channel_size i = channels[chan].future();
    const Future_wait& f = futures[i];

    if (f.complete(taskp, channels, chan))
        --nenqueued;

    return i;
}


optional<Channel_size>
Task::Future_selector::Future_set::pick_ready(const Future_wait_index& index, const Future_wait_vector& futures, const Channel_wait_vector& chans, Channel_size nready)
{
    optional<Channel_size> ready;

    if (nready > 0) {
        const Channel_size choice = random(1, nready);
        ready = get_ready(index, futures, chans, choice);
    }

    return ready;
}


void
Task::Future_selector::Future_set::remove_duplicates(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    const auto dup = unique(indexp->begin(), indexp->end(), [&](auto x, auto y) {
        return waits[x] == waits[y];
    });

    indexp->erase(dup, indexp->end());
}


optional<Channel_size>
Task::Future_selector::Future_set::select_ready()
{
    const auto n = count_ready(index, futures, channels);
    return pick_ready(index, futures, channels, n);
}


void
Task::Future_selector::Future_set::sort(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    std::sort(indexp->begin(), indexp->end(), [&](auto x, auto y) {
        return waits[x] < waits[y];
    });
}


void
Task::Future_selector::Future_set::unlock_channels()
{
    locks.release();
}


/*
    Task Future Selector Timer
*/
inline void
Task::Future_selector::Timer::cancel(Task::Promise* taskp) const
{
    scheduler.cancel_timer(taskp);
    state = cancel_pending;
}


inline bool
Task::Future_selector::Timer::is_active() const
{
    return state != inactive;
}


inline bool
Task::Future_selector::Timer::is_canceled() const
{
    return state == cancel_pending;
}


inline bool
Task::Future_selector::Timer::is_running() const
{
    return state == running;
}


inline void
Task::Future_selector::Timer::notify_canceled() const
{
    state = inactive;
}


inline void
Task::Future_selector::Timer::notify_expired(Time /*when*/) const
{
    state = inactive;
}


/*
    Task Future Selector
*/
inline bool
Task::Future_selector::is_ready(const Future_set& futures, Timer timer)
{
    return !(futures.enqueued() || timer.is_active());
}


bool
Task::Future_selector::notify_channel_readable(Task::Promise* taskp, Channel_size chan)
{
    const Channel_size f = futures.notify_readable(taskp, chan);

    if (quota > 0 && --quota == 0) {
        result = f;
        futures.dequeue(taskp);
        if (timer.is_running())
            timer.cancel(taskp);
    }

    return is_ready(futures, timer);
}


bool
Task::Future_selector::notify_timer_expired(Task::Promise* taskp, Time when)
{
    /*
        Timer expiration can race with cancellation, so treat expiration
        as a cancellation notification if cancellation was requested.
    */
    if (timer.is_canceled())
        timer.notify_canceled();
    else {
        timer.notify_expired(when);
        futures.dequeue(taskp);
        quota = 0;
    }

    return !futures.enqueued();
}


bool
Task::Future_selector::notify_timer_canceled()
{
    timer.notify_canceled();
    return !futures.enqueued();
}


/*
    Task Promise
*/
Task::Promise::Promise()
    : taskstate{State::ready}
{
}


inline void
Task::Promise::make_ready()
{
    taskstate = State::ready;
}


inline Task::State
Task::Promise::state() const
{
    return taskstate;
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
Channel_operation::dequeue(Task::Promise* taskp, Channel_size pos) const
{
    bool is_dequeued;

    if (chanp) {
        Task::Channel_lock lock(chanp);

        switch(kind) {
        case Type::send:
            is_dequeued = chanp->dequeue_write(taskp, pos);
            break;

        case Type::receive:
            is_dequeued = chanp->dequeue_read(taskp, pos);
            break;
        }
    }

    return is_dequeued;
}


void
Channel_operation::enqueue(Task::Promise* taskp, Channel_size pos) const
{
    if (chanp) {
        switch(kind) {
        case Type::send:
            if (rvalp)
                chanp->enqueue_write(taskp, pos, rvalp);
            else
                chanp->enqueue_write(taskp, pos, lvalp);
            break;

        case Type::receive:
            chanp->enqueue_read(taskp, pos, lvalp);
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
    Future "void" Awaitable
*/
void
Future<void>::Awaitable::await_resume()
{
    /*
        If a future is ready, the result can be obtained from one of its
        channels without waiting.  Otherwise, the task waiting on this
        future has just awakened from a call to select() having received
        either a value or an error.
    */
    if (selfp->is_ready())
        selfp->get_ready();
    else if (ep)
        rethrow_exception(ep);
}


bool
Future<void>::Awaitable::await_suspend(Task::Handle task)
{
    ops[0] = selfp->vchan.make_receive();
    ops[1] = selfp->echan.make_receive(&ep);
    task.promise().select(ops);
    return true;
}


/*
    Future of "void"
*/
void
Future<void>::get_ready()
{
    if (vchan.try_receive())
        isready = false;
    else if (optional<exception_ptr> epp = echan.try_receive()) {
        isready = false;
        rethrow_exception(*epp);
    }
}


bool
Future<void>::try_get()
{
    const bool is_value = vchan.try_receive();

    if (is_value)
        isready = false;
    else if (optional<exception_ptr> epp = echan.try_receive()) {
        isready = false;
        rethrow_exception(*epp);
    }

    return is_value;
}


bool
operator==(const Future<void>& x, const Future<void>& y)
{
    if (x.vchan != y.vchan) return false;
    if (x.echan != y.echan) return false;
    if (x.isready != y.isready) return false;
    return true;
}


void
swap(Future<void>& x, Future<void>& y)
{
    using std::swap;

    swap(x.vchan, y.vchan);
    swap(x.echan, y.echan);
    swap(x.isready, y.isready);
}


/*
    Scheduler Suspended Tasks
*/
inline
Scheduler::Waiting_tasks::handle_eq::handle_eq(Task::Handle task)
    : h{task}
{
}


inline bool
Scheduler::Waiting_tasks::handle_eq::operator()(const Task& task) const
{
    return task.handle() == h;
}


void
Scheduler::Waiting_tasks::insert(Task&& task)
{
    const Lock lock{mutex};

    tasks.push_back(move(task));
    tasks.back().unlock();
}


Task
Scheduler::Waiting_tasks::release(Task::Handle h)
{
    Task        task;
    const Lock  lock{mutex};
    const auto  taskp = find_if(tasks.begin(), tasks.end(), handle_eq(h));

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
    const Lock lock{mutex};

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
    const Lock lock{mutex};

    tasks.push_back(move(task));
    ready.notify_one();
}


Task
Scheduler::Task_queue::try_pop()
{
    Task        task;
    const Lock  lock{mutex, try_to_lock};

    if (lock && !tasks.empty())
        task = pop_front(&tasks);

    return task;
}


bool
Scheduler::Task_queue::try_push(Task&& task)
{
    bool        is_pushed{false};
    const Lock  lock{mutex, try_to_lock};

    if (lock) {
        tasks.push_back(move(task));
        ready.notify_one();
        is_pushed = true;
    }

    return is_pushed;
}


/*
    Scheduler Task Queues
*/
Scheduler::Task_queues::Task_queues(Size n)
    : qs{n}
{
}


void
Scheduler::Task_queues::interrupt()
{
    for (auto& q : qs)
        q.interrupt();
}


Task
Scheduler::Task_queues::pop(Size qpref)
{
    Task task;

    // Try to dequeue a task without waiting.
    for (Size i = 0, nqs = qs.size(); i < nqs && !task; ++i) {
        auto pos = (qpref + i) % nqs;
        task = qs[pos].try_pop();
    }

    // If that failed, wait on the preferred queue.
    if (!task)
        task = qs[qpref].pop();

    return task;
}


inline void
Scheduler::Task_queues::push(Task&& task)
{
    const auto nqs = qs.size();
    const auto qpref = nextq++ % nqs;

    push(&qs, qpref, move(task));
}


inline void
Scheduler::Task_queues::push(Size qpref, Task&& task)
{
    push(&qs, qpref, move(task));
}


void
Scheduler::Task_queues::push(Queue_vector* qvecp, Size qpref, Task&& task)
{
    Queue_vector&   qs  = *qvecp;
    const auto      nqs = qs.size();
    Size            i   = 0;

    // Try to enqueue the task without waiting.
    for (; i < nqs; ++i) {
        auto pos = (qpref + i) % nqs;
        if (qs[pos].try_push(move(task)))
            break;
    }

    // If that failed, wait on the preferred queue.
    if (i == nqs)
        qs[qpref].push(move(task));
}


inline Scheduler::Task_queues::Size
Scheduler::Task_queues::size() const
{
    return qs.size();
}


/*
    Scheduler Unlock Sentry
*/
inline
Scheduler::Unlock_sentry::Unlock_sentry(Lock* lp)
    : lockp{lp}
{
    lockp->unlock();
}


inline
Scheduler::Unlock_sentry::~Unlock_sentry()
{
    lockp->lock();
}


/*
    Scheduler Timers Alarm Queue
*/
inline Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::begin()
{
    return alarms.begin();
}


inline Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::end()
{
    return alarms.end();
}


inline void
Scheduler::Timers::Alarm_queue::erase(Iterator p)
{
    alarms.erase(p);
}


template<class T>
Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::find(const T& id)
{
    const auto first    = alarms.begin();
    const auto last     = alarms.end();
    const auto p        = find_if(first, last, id_eq(id));

    return (p != last) ? p : last;
}


inline const Scheduler::Timers::Alarm&
Scheduler::Timers::Alarm_queue::front() const
{
    return alarms.front();
}


inline Scheduler::Timers::Alarm_queue::Task_eq
Scheduler::Timers::Alarm_queue::id_eq(Task::Promise* taskp)
{
    return Task_eq{taskp};
}


inline Scheduler::Timers::Alarm_queue::Channel_eq
Scheduler::Timers::Alarm_queue::id_eq(const Time_channel& chan)
{
    return Channel_eq{chan};
}


inline bool
Scheduler::Timers::Alarm_queue::is_empty() const
{
    return alarms.empty();
}


inline Time
Scheduler::Timers::Alarm_queue::next_expiry() const
{
    return alarms.front().time;
}


inline Scheduler::Timers::Alarm
Scheduler::Timers::Alarm_queue::pop()
{
    Alarm a = move(alarms.front());

    alarms.pop_front();
    return a;
}


Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::push(const Alarm& a)
{
    const auto p = upper_bound(alarms.begin(), alarms.end(), a);
    return alarms.insert(p, a);
}


void
Scheduler::Timers::Alarm_queue::reschedule(Iterator p, Time time)
{
    p->time = time;
    sort(alarms.begin(), alarms.end());
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

    /*
        TODO:  Implement error handling for Windows APIs (perhaps using
        std::system_error).
    */
    try {
        assert(timer_handle == n);
        hs[n++] = CreateWaitableTimer(NULL, FALSE, NULL);

        assert(interrupt_handle == n);
        hs[n++] = CreateEvent(NULL, FALSE, FALSE, NULL);

        assert(n == count);
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
        CloseHandle(hs[n-i - 1]);
}


inline void
Scheduler::Timers::Windows_handles::signal_interrupt() const
{
    SetEvent(hs[interrupt_handle]);
}


inline Scheduler::Timers::Windows_handle
Scheduler::Timers::Windows_handles::timer() const
{
    return hs[timer_handle];
}


inline int
Scheduler::Timers::Windows_handles::wait_any(Lock* lockp) const 
{
    const Unlock_sentry unlock{lockp};
    const DWORD         n = WaitForMultipleObjects(count, hs, FALSE, INFINITE);

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
Scheduler::Timers::cancel(Task::Promise* taskp)
{
    sync_cancel(taskp);
}


bool
Scheduler::Timers::cancel(Task::Promise* taskp, Alarm_queue::Iterator alarmp, Alarm_queue* queuep, Windows_handle timer)
{
    remove_canceled(alarmp, queuep, timer);
    if (taskp->notify_timer_canceled())
        scheduler.resume(taskp);

    return true;
}


bool
Scheduler::Timers::cancel(Time_channel chan, Alarm_queue::Iterator alarmp, Alarm_queue* queuep, Windows_handle timer)
{
    remove_canceled(alarmp, queuep, timer);
    return chan.is_empty();
}


inline void
Scheduler::Timers::cancel_timer(Windows_handle handle)
{
    CancelWaitableTimer(handle);
}


inline bool
Scheduler::Timers::is_ready(const Alarm& alarm, Time now)
{
    return alarm.time <= now;
}


inline bool
Scheduler::Timers::notify_timer_expired(Task::Promise* taskp, Time now, Lock* lockp)
{
    /*
        Timer cancellation can race with timer expiration, so unlock the
        timers before notifying the task that its timer has expired.
    */
    const Unlock_sentry unlock{lockp};
    return taskp->notify_timer_expired(now);
}


void
Scheduler::Timers::process_ready(Alarm_queue* queuep, Windows_handle timer, Lock* lockp)
{
    const auto now = Clock::now();

    signal_ready(queuep, now, lockp);
    if (!queuep->is_empty())
        set_timer(timer, queuep->front(), now);
}


void
Scheduler::Timers::remove_canceled(Alarm_queue::Iterator alarmp, Alarm_queue* queuep, Windows_handle timer)
{
    // If the alarm is next to fire, update the timer.
    if (alarmp == queuep->begin()) {
        const auto nextp = next(alarmp);
        if (nextp == queuep->end())
            cancel_timer(timer);
        else if (alarmp->time < nextp->time)
            set_timer(timer, *nextp, Clock::now());
    }

    queuep->erase(alarmp);
}


void
Scheduler::Timers::reschedule(Alarm_queue::Iterator alarmp, Duration duration, Alarm_queue* queuep, Windows_handle timer)
{
    const auto old = queuep->next_expiry();
    const auto now = Clock::now();
    
    queuep->reschedule(alarmp, now + duration);
    if (queuep->next_expiry() != old)
        set_timer(timer, queuep->front(), now);
}


inline bool
Scheduler::Timers::reset(const Time_channel& chan, Duration duration)
{
    bool        is_reset{false};
    const Lock  lock{mutex};
    const auto  alarmp{alarmq.find(chan)};

    if (alarmp == alarmq.end())
        start_alarm(chan, duration, &alarmq, handles.timer());
    else {
        reschedule(alarmp, duration, &alarmq, handles.timer());
        if (!chan.try_receive())
            is_reset = true;
    }
 
    return is_reset;
}


void
Scheduler::Timers::run_thread()
{
    bool done{false};
    Lock lock{mutex};

    while (!done) {
        switch(handles.wait_any(&lock)) {
        case timer_handle:
            process_ready(&alarmq, handles.timer(), &lock);
            break;

        default:
            done = true;
            break;
        }
    }
}


void
Scheduler::Timers::set_timer(Windows_handle timer, const Alarm& alarm, Time now)
{
    static const int nanosecs_per_tick = 100;

    const Duration      dt      = alarm.time - now;
    const std::int64_t  timerdt = -(dt.count() / nanosecs_per_tick);
    LARGE_INTEGER       timebuf;

    timebuf.LowPart = static_cast<DWORD>(timerdt & 0xFFFFFFFF);
    timebuf.HighPart = static_cast<LONG>(timerdt >> 32);
    SetWaitableTimer(timer, &timebuf, 0, NULL, NULL, FALSE);
}


inline void
Scheduler::Timers::signal_alarm(Task::Promise* taskp, Time now, Lock* lockp)
{
    if (notify_timer_expired(taskp, now, lockp))
        scheduler.resume(taskp);
}


inline void
Scheduler::Timers::signal_alarm(const Time_channel& chan, Time now)
{
    chan.try_send(now);
}


void
Scheduler::Timers::signal_ready(const Alarm& alarm, Time now, Lock* lockp)
{
    if (alarm.taskp)
        signal_alarm(alarm.taskp, now, lockp);
    else
        signal_alarm(alarm.channel, now);
}


void
Scheduler::Timers::signal_ready(Alarm_queue* qp, Time now, Lock* lockp)
{
    while (!qp->is_empty() && is_ready(qp->front(), now))
        signal_ready(qp->pop(), now, lockp);
}


inline void
Scheduler::Timers::start(Task::Promise* taskp, Duration duration)
{
    sync_start(taskp, duration);
}


inline void
Scheduler::Timers::start(const Time_channel& chan, Duration duration)
{
    sync_start(chan, duration);
}


template<class T>
inline void
Scheduler::Timers::start_alarm(const T& id, Duration duration, Alarm_queue* queuep, Windows_handle timer)
{
    const auto now      = Clock::now();
    const auto expiry   = now + duration;
    const auto alarmp   = queuep->push(Alarm{id, expiry});

    if (alarmp == queuep->begin())
        set_timer(timer, *alarmp, now);
}


inline bool
Scheduler::Timers::stop(const Time_channel& chan)
{
    return sync_cancel(chan);
}


template<class T>
inline bool
Scheduler::Timers::sync_cancel(const T& id)
{
    bool        is_cancel{false};
    const Lock  lock{mutex};
    const auto  alarmp{alarmq.find(id)};

    if (alarmp != alarmq.end())
        is_cancel = cancel(id, alarmp, &alarmq, handles.timer());

    return is_cancel;
}


template<class T>
inline void
Scheduler::Timers::sync_start(const T& id, Duration duration)
{
    const Lock lock{mutex};

    start_alarm(id, duration, &alarmq, handles.timer());
}


/*
    Scheduler
*/
Scheduler::Scheduler(int nthreads)
    : ready{nthreads > 0 ? nthreads : Thread::hardware_concurrency()}
{
    const auto nqs = ready.size();

    threads.reserve(nqs);
    for (unsigned q = 0; q != nqs; ++q)
        threads.emplace_back([&,q]{ run_tasks(q); });
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
    for (auto& thread : threads)
        thread.join();
}


void
Scheduler::cancel_timer(Task::Promise* taskp)
{
    timers.cancel(taskp);
}


bool
Scheduler::reset_timer(const Time_channel& chan, Duration duration)
{
    return timers.reset(chan, duration);
}


void
Scheduler::resume(Task::Promise* taskp)
{
    auto task = Task::Handle::from_promise(*taskp);
    ready.push(waiting.release(task));
}


void
Scheduler::run_tasks(unsigned q)
{
    while (Task task = ready.pop(q)) {
        try {
            switch(task.resume()) {
            case Task::State::ready:
                ready.push(q, move(task));
                break;

            case Task::State::waiting:
                waiting.insert(move(task));
                break;
            }
        } catch (...) {
            ready.interrupt();
        }
    }
}


void
Scheduler::start_timer(Task::Promise* taskp, Duration duration)
{
    timers.start(taskp, duration);
}


void
Scheduler::start_timer(const Time_channel& chan, Duration duration)
{
    timers.start(chan, duration);
}


bool
Scheduler::stop_timer(const Time_channel& chan)
{
    return timers.stop(chan);
}


void
Scheduler::submit(Task task)
{
    ready.push(move(task));
}


/*
    Timer
*/
Timer::Timer(Duration duration)
    : chan{is_valid(duration) ? make_timer(&scheduler, duration) : Time_channel()}
{
}


inline bool
Timer::is_valid(Duration x)
{
    return x >= 0ns;
}


inline Time_channel
Timer::make_timer(Scheduler* schedp, Duration duration)
{
    Time_channel chan = make_channel<Time>(1);

    scheduler.start_timer(chan, duration);
    return chan;
}


bool
Timer::reset(Duration duration)
{
    bool is_reset = false;

    if (is_valid(duration)) {
        if (!chan)
            chan = make_timer(&scheduler, duration);
        else if (scheduler.reset_timer(chan, duration))
            is_reset = true;
    }

    return is_reset;
}


}   // Coroutine
}   // Isptech

//  $CUSTOM_FOOTER$
