//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/channel.cpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/channel.cpp,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


#include "isptech/concurrency/channel.hpp"
#include <algorithm>

#pragma warning(disable: 4073)
#pragma init_seg(lib)

/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


// Names/Types
Scheduler scheduler;


/*
    Names/Types
*/
using std::find_if;
using std::move;
using std::try_to_lock;


/*
    Implementation Details
*/
namespace Detail {
    
    
/*
    Channel Alternatives
*/
template<Channel_size N>
Channel_alternatives::Channel_alternatives(Channel_operation (&ops)[N])
    : first{begin(ops)}
    , last{end(ops)}
{
    save_positions(first, last);
}


void
Channel_alternatives::complete(Channel_size pos, Goroutine::Handle g)
{
    for (Channel_operation* op = first; op != last; ++op) {
        if (op - first != pos)
            op->dequeue(g);
    }

    chosen = pos;
}


inline Channel_size
Channel_alternatives::count_ready(Channel_operation* first, Channel_operation* last)
{
    return std::count_if(first, last, is_ready());
}


void
Channel_alternatives::enqueue(Goroutine::Handle g)
{
    enqueue(first, last, g, this);
    unlock(first, last);
}


void
Channel_alternatives::enqueue(Channel_operation* first, Channel_operation* last, Goroutine::Handle g, Channel_alternatives* selfp)
{
    for (Channel_operation* op = first; op != last; ++op)
        op->enqueue(g, selfp);
}


inline void
Channel_alternatives::lock(Channel_operation* first, Channel_operation* last)
{
    sort_channels(first, last);
    lock_channels(first, last);
}
    
   
void
Channel_alternatives::lock_channels(Channel_operation* first, Channel_operation* last)
{
    Channel_operation::Interface* prevchanp = nullptr;

    for (Channel_operation* op = first; op != last; ++op) {
        // lock each channel just once
        if (op->chanp && op->chanp != prevchanp) {
            op->chanp->lock();
            prevchanp = op->chanp;
        }
    }
}
    
   
Channel_operation*
Channel_alternatives::pick_ready(Channel_operation* first, Channel_operation* last, Channel_size nready)
{
    Channel_operation*  readyp  = last;
    Channel_size        n       = random(1, nready);

    for (Channel_operation* op = first; op != last; ++op) {
        if (op->is_ready() && --n == 0) {
            readyp = op;
            break;
        }
    }

    return readyp;
}


Channel_size
Channel_alternatives::random(Channel_size min, Channel_size max)
{
    using Device        = std::random_device;
    using Engine        = std::default_random_engine;
    using Distribution  = std::uniform_int_distribution<Channel_size>;

    Device          rand;
    Engine          engine{rand()};
    Distribution    dist{min, max};

    return dist(engine);
}
    
   
inline void
Channel_alternatives::reposition(Channel_operation* first, Channel_operation* last)
{
    using std::sort;

    sort(first, last, position_less());
}


void
Channel_alternatives::save_positions(Channel_operation* first, Channel_operation* last)
{
    int i = 0;

    for (Channel_operation* op = first; op != last; ++op)
        op->pos = i++;
}


optional<Channel_size>
Channel_alternatives::select()
{
    lock(first, last);
    chosen = select_ready(first, last);

    // If an operation was selected, unlock channels; otherwise, they will
    // be unlocked after all operations have been enqueued.
    if (chosen)
        unlock(first, last);

    return chosen;
}


optional<Channel_size>
Channel_alternatives::select_ready(Channel_operation* first, Channel_operation* last)
{
    optional<Channel_size>  pos;
    const Channel_size      n = count_ready(first, last);

    if (n > 0) {
        Channel_operation* op = pick_ready(first, last, n);
        op->execute();
        pos = op->pos;
    }

    return pos;
}


void
Channel_alternatives::sort_channels(Channel_operation* first, Channel_operation* last)
{
    using std::sort;

    sort(first, last, channel_less());
}


void
Channel_alternatives::unlock(Channel_operation* first, Channel_operation* last)
{
    Channel_operation::Interface* prevchanp = nullptr;

    for (Channel_operation* op = first; op != last; ++op) {
        // unlock each channel just once
        if (op->chanp && op->chanp != prevchanp) {
            op->chanp->unlock();
            prevchanp = op->chanp;
        }
    }

    reposition(first, last);
}


inline bool
Channel_alternatives::is_ready::operator()(const Channel_operation& op) const
{
    return op.is_ready();
}


inline bool
Channel_alternatives::channel_less::operator()(const Channel_operation& x, const Channel_operation& y) const
{
    return x.chanp < y.chanp;
}


inline bool
Channel_alternatives::position_less::operator()(const Channel_operation& x, const Channel_operation& y) const
{
    return x.pos < y.pos;
}
    
   
/*
    Channel Selection
*/
bool
select(Channel_alternatives* altsp, Goroutine::Handle g)
{
    bool is_selected = true;

    if (!altsp->select()) {
        altsp->enqueue(g);
        scheduler.suspend(g);
        is_selected = false;
    }

    return is_selected;
}


/*
    Sheduler Work Queue
*/
void
Workqueue::interrupt()
{
    Lock lock{mutex};

    is_interrupt = true;
    lock.unlock();
    ready.notify_all();
}


optional<Goroutine>
Workqueue::pop()
{
    optional<Goroutine> gp;
    Lock                lock{mutex};

    while (q.is_empty() && !is_interrupt)
        ready.wait(lock);

    if (!q.is_empty())
        gp = q.pop();

    return gp;
}


void
Workqueue::push(Goroutine&& g)
{
    push(mutex, move(g), &q);
    ready.notify_one();
}


inline void
Workqueue::push(Mutex& sync, Goroutine&& g, Goroutine_queue* qp)
{
    Lock lock{sync};

    qp->push(move(g));
}


optional<Goroutine>
Workqueue::try_pop()
{
    optional<Goroutine> gp;
    Lock                lock{mutex, try_to_lock};

    if (lock && !q.is_empty())
        gp = q.pop();

    return gp;
}


bool
Workqueue::try_push(Goroutine&& g)
{
    bool is_done{false};
    Lock lock{mutex, try_to_lock};

    if (lock) {
        q.push(move(g));
        lock.unlock();
        ready.notify_one();
        is_done = true;
    }

    return is_done;
}


/*
    Work Queue Goroutine Queue
*/
inline bool
Workqueue::Goroutine_queue::is_empty() const
{
    return elems.empty();
}


inline Goroutine
Workqueue::Goroutine_queue::pop()
{
    Goroutine g{move(elems.front())};

    elems.pop_front();
    return g;
}


inline void
Workqueue::Goroutine_queue::push(Goroutine&& g)
{
    elems.push_back(move(g));
}


/*
    Scheduler Work Queue Array
*/
Workqueue_array::Workqueue_array(Size n)
    : queues{n}
{
}


void
Workqueue_array::interrupt()
{
    for (auto& q : queues)
        q.interrupt();
}


optional<Goroutine>
Workqueue_array::pop(Size preferred)
{
    const auto          nqueues = queues.size();
    optional<Goroutine> gp;

    // Beginning with the preferred queue, try to dequeue work without waiting.
    for (Size i = 0; !gp && i < nqueues; ++i) {
        auto pos = (preferred + i) % nqueues;
        gp = queues[pos].try_pop();
    }

    // If we failed, wait on the preferred queue.
    if (!gp)
        gp = queues[preferred].pop();

    return gp;
}


void
Workqueue_array::push(Goroutine&& g)
{
    const auto  nqueues     = queues.size();
    const auto  preferred   = nextqueue++ % nqueues;
    bool        is_enqueued = false;

    // Beginning with the preferred queue, try to enqueue work without waiting.
    for (Size i = 0; !is_enqueued && i < nqueues; ++i) {
        auto pos = (preferred + i) % nqueues;
        if (queues[pos].try_push(move(g)))
            is_enqueued = true;
    }

    // If we failed, wait on the preferred queue.
    if (!is_enqueued)
        queues[preferred].push(move(g));
}


inline Workqueue_array::Size
Workqueue_array::size() const
{
    return queues.size();
}


/*
    Goroutine List
*/
inline
Goroutine_list::handle_equal::handle_equal(Goroutine::Handle gh)
    : h{gh}
{
}


inline bool
Goroutine_list::handle_equal::operator()(const Goroutine& g) const
{
    return g.handle() == h;
}


void
Goroutine_list::insert(Goroutine&& g)
{
    Lock lock{mutex};

    gs.push_back(move(g));
}


Goroutine
Goroutine_list::release(Goroutine::Handle h)
{
    Goroutine   g;
    Lock        lock{mutex};
    auto        p = find_if(gs.begin(), gs.end(), handle_equal(h));

    if (p != gs.end()) {
        g = move(*p);
        gs.erase(p);
    }

    return g;
}


}   // Implementation Details


/*
    Channel Operation
*/
Channel_operation::Channel_operation()
    : kind{none}
    , chanp{nullptr}
    , rvalp{nullptr}
    , lvalp{nullptr}
    , pos{0}
{
}


Channel_operation::Channel_operation(Interface* channelp, const void* rvaluep)
    : kind{send}
    , chanp{channelp}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , pos{0}
{
}


Channel_operation::Channel_operation(Interface* channelp, Type optype, void* lvaluep)
    : kind{optype}
    , chanp{channelp}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , pos{0}
{
}


void
Channel_operation::dequeue(Goroutine::Handle g)
{
    if (chanp && g) {
        chanp->lock();

        switch(kind) {
        case send:
            if (rvalp)
                chanp->dequeue_send(g);
            else if (lvalp)
                chanp->dequeue_send(g);
            break;

        case receive:
            if (lvalp)
                chanp->dequeue_receive(g);
            break;
        }

        chanp->unlock();
    }
}


void
Channel_operation::enqueue(Goroutine::Handle g, Detail::Channel_alternatives* altsp)
{
    if (chanp && g) {
        switch(kind) {
        case send:
            if (rvalp)
                chanp->enqueue_send(altsp, pos, g, rvalp);
            else if (lvalp)
                chanp->enqueue_send(altsp, pos, g, lvalp);
            break;

        case receive:
            if (lvalp)
                chanp->enqueue_receive(altsp, pos, g, lvalp);
            break;
        }
    }
}


void
Channel_operation::execute()
{
    if (chanp) {
        switch(kind) {
        case send:
            if (rvalp)
                chanp->ready_send(rvalp);
            else if (lvalp)
                chanp->ready_send(lvalp);
            break;

        case receive:
            if (lvalp)
                chanp->ready_receive(lvalp);
            break;
        }
    }
}


bool
Channel_operation::is_ready() const
{
    if (!chanp) return false;

    switch(kind) {
    case send:      return chanp->is_send_ready(); break;
    case receive:   return chanp->is_receive_ready(); break;
    default:        return false;
    }
}


/*
    Goroutine Scheduler
*/
Scheduler::Scheduler(int nthreads)
    : workqueues{nthreads > 0 ? nthreads : thread::hardware_concurrency()}
{
    const auto nqueues = workqueues.size();

    workers.reserve(nqueues);
    for (unsigned q = 0; q != nqueues; ++q)
        workers.emplace_back([&,q]{ run_work(q); });
}


Scheduler::~Scheduler()
{
    /*
        TODO:  Could this destructor should be rendered unnecessary because
        by arranging for (a) queues to shutdown implicitly (in their
        destructors) and (b) workers to be joined implicitly (in their
        destructors)?
    */
    workqueues.interrupt();
    for (auto& w : workers) w.join();
}


void
Scheduler::resume(Goroutine::Handle h)
{
    workqueues.push(suspended.release(h));
}


void
Scheduler::run_work(unsigned threadpos)
{
    while (optional<Goroutine> gp = workqueues.pop(threadpos)) {
        try {
            gp->run();
        } catch (...) {
            workqueues.interrupt();
        }
    }
}


void
Scheduler::submit(Goroutine&& g)
{
    workqueues.push(move(g));
}


void
Scheduler::suspend(Goroutine::Handle h)
{
    suspended.insert(Goroutine(h));
}


}   // Concurrency
}   // Isptech
