//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/channel.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/channel.inl,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Goroutine
*/
inline
Goroutine::Goroutine(Handle h)
    : coro{h}
    , isowner{true}
{
}


inline
Goroutine::Goroutine(Goroutine&& other)
{
    steal(&other);
}


inline Goroutine&
Goroutine::operator=(Goroutine&& other)
{
    destroy(this);
    steal(&other);
    return *this;
}


inline
Goroutine::~Goroutine()
{
    destroy(this);
}


inline void
Goroutine::destroy(Goroutine* gp)
{
    if (gp->coro && gp->isowner)
        gp->coro.destroy();
}


inline Goroutine::Handle
Goroutine::handle() const
{
    return coro;
}


inline bool
Goroutine::is_owner() const
{
    return isowner;
}


inline
Goroutine::operator bool() const
{
    return coro ? true : false;
}


inline void
Goroutine::release()
{
    coro    = nullptr;
    isowner = false;
}


inline void
Goroutine::reset(Handle h)
{
    destroy(this);
    coro = h;
    isowner = true;
}


inline void
Goroutine::run()
{
    coro.resume();
    if (!coro.promise().is_done())
        release();
}


inline void
Goroutine::steal(Goroutine* otherp)
{
    coro    = otherp->coro;
    isowner = otherp->isowner;
    otherp->release();
}


inline bool
operator==(const Goroutine& x, const Goroutine& y)
{
    if (x.coro != y.coro) return false;
    if (x.isowner != y.isowner) return false;
    return true;
}


inline bool
Goroutine::Final_suspend::await_ready()
{
    return false;
}


inline void
Goroutine::Final_suspend::await_resume()
{
}


inline bool
Goroutine::Final_suspend::await_suspend(Goroutine::Handle coro)
{
    coro.promise().done();
    return true;
}


/*
    Goroutine Promise
*/
inline
Goroutine::Promise::Promise()
    : isdone{false}
{
}


inline void
Goroutine::Promise::done()
{
    isdone = true;
}


inline Goroutine::Final_suspend
Goroutine::Promise::final_suspend() const
{
    return Final_suspend{};
}


inline Goroutine
Goroutine::Promise::get_return_object()
{
    return Goroutine(Handle::from_promise(*this));
}


inline Goroutine::Initial_suspend
Goroutine::Promise::initial_suspend() const
{
    return Initial_suspend{};
}


inline bool
Goroutine::Promise::is_done() const
{
    return isdone;
}


inline void
Goroutine::Promise::return_void()
{
}


/*
    Channel Operation
*/
inline bool
operator==(const Channel_operation& x, const Channel_operation& y)
{
    if (x.chanp != y.chanp) return false;
    if (x.kind != y.kind) return false;
    if (x.lvalp != y.lvalp) return false;
    if (x.rvalp != y.rvalp) return false;
    return true;
}


inline bool
operator< (const Channel_operation& x, const Channel_operation& y)
{
    return x.chanp < y.chanp;
}


/*
    Channel
*/
template<class T>
inline
Channel<T>::Channel()
    : pimpl{std::move(p)}
{
}


template<class T>
inline
Channel<T>::Channel(Impl_ptr p)
    : pimpl{std::move(p)}
{
}


template<class T>
inline Channel_size
Channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline Channel_operation
Channel<T>::make_receivable(T* valuep) const
{
    return pimpl->make_receivable(valuep);
}


template<class T>
inline Channel_operation
Channel<T>::make_sendable(const T& value) const
{
    return pimpl->make_sendable(&value);
}


template<class T>
inline Channel_operation
Channel<T>::make_sendable(T&& value) const
{
    return pimpl->make_sendable(&value);
}


template<class T>
inline Channel<T>&
Channel<T>::operator=(Channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


template<class T>
inline
Channel<T>::operator bool() const
{
    return pimpl;
}


template<class T>
inline typename Channel<T>::Receive_awaitable
Channel<T>::receive() const
{
    return pimpl->receive();
}


template<class T>
inline typename Channel<T>::Send_awaitable
Channel<T>::send(const T& value) const
{
    return pimpl->awaitable_send(&value);
}


template<class T>
inline typename Channel<T>::Send_awaitable
Channel<T>::send(T&& value) const
{
    return pimpl->send(&value);
}


template<class T>
inline Channel_size
Channel<T>::size() const
{
    return pimpl->size();
}


template<class T>
inline T
Channel<T>::sync_receive() const
{
    return pimpl->sync_receive();
}


template<class T>
inline void
Channel<T>::sync_send(const T& value) const
{
    return pimpl->sync_send(&value);
}


template<class T>
inline void
Channel<T>::sync_send(T&& value) const
{
    return pimpl->sync_send(&value);
}


template<class T>
inline optional<T>
Channel<T>::try_receive() const
{
    return pimpl->try_receive();
}


template<class T>
inline bool
Channel<T>::try_send(const T& value) const
{
    return pimpl->try_send(value);
}


template<class T>
inline bool
operator==(const Channel<T>& x, const Channel<T>& y)
{
    return x.pimpl != y.pimpl;
}


template<class T>
inline bool
operator< (const Channel<T>& x, const Channel<T>& y)
{
    return x.pimpl < y.pimpl;
}


template<class T>
inline void
swap(Channel<T>& x, Channel<T>& y)
{
    using std::swap;
    swap(x.pimpl, y.pimpl);
}


/*
    Channel Implementation
*/
template<class T>
inline
Channel<T>::Impl::Impl(Channel_size n)
    : buffer{n}
{
}


template<class T>
typename Channel<T>::Receive_awaitable
Channel<T>::Impl::awaitable_receive()
{
    return Receive_awaitable(this);
}


template<class T>
template<class U>
typename Channel<T>::Send_awaitable
Channel<T>::Impl::awaitable_send(U* valuep)
{
    return Send_awaitable(this, valuep);
}


template<class T>
Channel_size
Channel<T>::Impl::capacity() const
{
    return buffer.capacity();
}


template<class T>
template<class U>
void
Channel<T>::Impl::dequeue_receiver(Receiver_queue* waitqp, U* valuep)
{
    Waiting_receiver r = waitqp->pop();
    r.resume(valuep);
}


template<class T>
inline void
Channel<T>::Impl::dequeue_sender(Sender_queue* waitqp, optional<T>* valuep)
{
    T value;

    dequeue_sender(waitqp, &value);
    *valuep = value;
}


template<class T>
inline void
Channel<T>::Impl::dequeue_sender(Sender_queue* waitqp, T* valuep)
{
    Waiting_sender s = waitqp->pop();
    s.resume(valuep);
}


template<class T>
void
Channel<T>::Impl::enqueue_receive(Goroutine::Handle g, void* valuep)
{
    enqueue_receive(g, static_cast<T*>(valuep));
}


template<class T>
inline void
Channel<T>::Impl::enqueue_receive(Goroutine::Handle g, T* valuep)
{
    const Waiting_receiver r{g, valuep};
    receiverq.push(r);
}


template<class T>
void
Channel<T>::Impl::enqueue_send(Goroutine::Handle g, const void* rvaluep)
{
    enqueue_send(g, static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::enqueue_send(Goroutine::Handle g, void* lvaluep)
{
    enqueue_send(g, static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::enqueue_send(Goroutine::Handle g, U* valuep)
{
    const Waiting_sender s{g, valuep};
    senderq.push(s);
}


template<class T>
bool
Channel<T>::Impl::is_receivable() const
{
    return !(buffer.is_empty() || senderq.is_empty());
}


template<class T>
bool
Channel<T>::Impl::is_sendable() const
{
    return !(buffer.is_full() || receiverq.is_empty());
}


template<class T>
void
Channel<T>::Impl::lock()
{
    mutex.lock();
}


template<class T>
inline Channel_operation
Channel<T>::Impl::make_sendable(T* valuep)
{
    return Channel_operation(this, Channel_operation::send, valuep);
}


template<class T>
inline Channel_operation
Channel<T>::Impl::make_sendable(const T* valuep)
{
    return Channel_operation(this, valuep);
}


template<class T>
inline Channel_operation
Channel<T>::Impl::make_receivable(T* valuep)
{
    return Channel_operation(this, Channel_operation::receive, valuep);
}


template<class T>
void
Channel<T>::Impl::pop_value(Buffer* bufp, T* valuep, Sender_queue* waitqp)
{
    bufp->pop(valuep);

    if (!waitqp->is_empty()) {
        Waiting_sender s = waitqp->pop();
        s.resume(bufp);
    }
}


template<class T>
void
Channel<T>::Impl::ready_receive(void* valuep)
{
    ready_receive(static_cast<T*>(valuep));
}


template<class T>
void
Channel<T>::Impl::ready_receive(T* valuep)
{
    if (!buffer.is_empty())
        pop_value(&buffer, valuep, &senderq);
    else
        dequeue_sender(&senderq, valuep);
}


template<class T>
void
Channel<T>::Impl::ready_send(const void* rvaluep)
{
    ready_send(static_cast<const T*>(rvaluep));
}


template<class T>
void
Channel<T>::Impl::ready_send(void* lvaluep)
{
    ready_send(static_cast<T*>(lvaluep));
}


template<class T>
template<class U>
void
Channel<T>::Impl::ready_send(U* valuep)
{
    if (!receiverq.is_empty())
        dequeue_receiver(&receiverq, valuep);
    else
        buffer.push(move(*valuep));
}


template<class T>
void
Channel<T>::Impl::send(const void* rvaluep)
{
    send(static_cast<const T*>(rvaluep));
}


template<class T>
inline Channel_size
Channel<T>::Impl::size() const
{
    return buffer.size();
}


template<class T>
inline T
Channel<T>::Impl::sync_receive()
{
    T       value;
    Lock    lock{mutex};

    if (!receiverq.is_empty())
        wait_for_sender(lock, &receiverq, &value);
    else if (!buffer.is_empty())
        pop_value(&buffer, &value, &senderq);
    else if (!senderq.is_empty())
        dequeue_sender(&senderq, &value);
    else
        wait_for_sender(lock, &receiverq, &value);

    return value;
}


template<class T>
template<class U>
inline void
Channel<T>::Impl::sync_send(U* valuep)
{
    Lock lock{mutex};

    if (!senderq.is_empty())
        wait_for_receiver(lock, &senderq, valuep);
    else if (!receiverq.is_empty())
        dequeue_receiver(&receiverq, valuep);
    else if (!buffer.is_full())
        buffer.push(move(*valuep));
    else
        wait_for_receiver(lock, &senderq, valuep);
}


template<class T>
inline optional<T>
Channel<T>::Impl::try_receive()
{
    optional<T> value;
    Lock        lock{mutex};

    if (!buffer.is_empty())
        buffer.pop(&value);
    else if (!senderq.is_empty())
        dequeue_sender(&senderq, &value);

    return value;
}


template<class T>
inline bool
Channel<T>::Impl::try_send(const T& value)
{
    bool is_sent{true};
    Lock lock{mutex};

    if (!receiverq.is_empty())
        dequeue_receiver(&receiverq, &value);
    else if (!buffer.is_full())
        buffer.push(value);
    else
        is_sent = false;

    return is_sent;
}


template<class T>
void
Channel<T>::Impl::unlock()
{
    mutex.unlock();
}


template<class T>
template<class U>
void
Channel<T>::Impl::wait_for_receiver(Lock& lock, Sender_queue* waitqp, U* valuep)
{
    condition_variable      ready;
    const Waiting_sender    sender{&ready, valuep};

    waitqp->push(sender);
    ready.wait(lock, [&]{ return !waitqp->find(sender); });
}


template<class T>
void
Channel<T>::Impl::wait_for_sender(Lock& lock, Receiver_queue* waitqp, T* valuep)
{
    condition_variable      ready;
    const Waiting_receiver  receiver{&ready, valuep};

    waitqp->push(receiver);
    ready.wait(lock, [&]{ return !waitqp->find(receiver); });
}


/*
    Channel Receive Awaitable
*/
template<class T>
inline
Channel<T>::Receive_awaitable::Receive_awaitable(Impl* channelp)
{
    ops[0] = channelp->make_receivable(&value);
}


template<class T>
inline bool
Channel<T>::Receive_awaitable::await_ready()
{
    return false;
}


template<class T>
inline bool
Channel<T>::Receive_awaitable::await_suspend(Goroutine::Handle g)
{
    using Detail::Channel_alternatives;

    Channel_alternatives    alternatives;
    bool                    is_suspended = false;

    if (alternatives.select(ops)) {
        alternatives.enqueue(g);
        scheduler.suspend(g);
        is_suspended = true;
    }

    return is_suspended;
}


template<class T>
inline T&&
Channel<T>::Receive_awaitable::await_resume()
{
    return std::move(value);
}


/*
    Channel Send Awaitable
*/
template<class T>
template<class U>
inline
Channel<T>::Send_awaitable::Send_awaitable(Impl* channelp, U* valuep)
{
    ops[0] = channelp->make_sendable(valuep);
}


template<class T>
inline bool
Channel<T>::Send_awaitable::await_ready()
{
    return false;
}


template<class T>
inline void
Channel<T>::Send_awaitable::await_resume()
{
}


template<class T>
bool
Channel<T>::Send_awaitable::await_suspend(Goroutine::Handle g)
{
    using Detail::Channel_alternatives;

    Channel_alternatives    alternatives;
    bool                    is_suspended = false;

    if (alternatives.select(ops)) {
        alternatives.enqueue(g);
        scheduler.suspend(g);
        is_suspended = true;
    }

    return is_suspended;
}


/*
    Receive Channel
*/
template<class T>
inline
Receive_channel<T>::Receive_channel()
{
}


template<class T>
inline
Receive_channel<T>::Receive_channel(const Channel<T>& chan)
    : pimpl(chan.pimpl)
{
}


template<class T>
inline Channel_size
Receive_channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline Channel_operation
Receive_channel<T>::make_receivable(T* valuep)
{
    return pimpl->make_receivable(valuep);
}


template<class T>
inline Receive_channel<T>&
Receive_channel<T>::operator=(Receive_channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


template<class T>
inline Receive_channel<T>&
Receive_channel<T>::operator=(const Channel<T>& other)
{
    pimpl = other.pimpl;
    return *this;
}


template<class T>
inline
Receive_channel<T>::operator bool() const
{
    return pimpl;
}


template<class T>
inline typename Receive_channel<T>::Awaitable
Receive_channel<T>::receive() const
{
    return pimpl->awaitable_receive();
}


template<class T>
inline Channel_size
Receive_channel<T>::size() const
{
    return pimpl->size();
}


template<class T>
inline T
Receive_channel<T>::sync_receive() const
{
    return pimpl->sync_receive();
}


template<class T>
inline optional<T>
Receive_channel<T>::try_receive() const
{
    return pimpl->try_receive();
}


template<class T>
inline bool
operator==(const Receive_channel<T>& x, const Receive_channel<T>& y)
{
    return x.pimpl != y.pimpl;
}


template<class T>
inline bool
operator< (const Receive_channel<T>& x, const Receive_channel<T>& y)
{
    return x.pimpl < y.pimpl;
}


template<class T>
inline void
swap(Receive_channel<T>& x, Receive_channel<T>& y)
{
    using std::swap;
    swap(x.pimpl, y.pimpl);
}


/*
    Send Channel
*/
template<class T>
inline
Send_channel<T>::Send_channel()
{
}


template<class T>
inline
Send_channel<T>::Send_channel(const Channel<T>& other)
    : pimpl{other.pimpl}
{
}


template<class T>
inline Channel_size
Send_channel<T>::capacity() const
{
    return pimpl->capacity();
}


template<class T>
inline Channel_operation
Send_channel<T>::make_sendable(const T& value) const
{
    return pimpl->make_sendable(&value);
}


template<class T>
inline Channel_operation
Send_channel<T>::make_sendable(T&& value) const
{
    return pimpl->make_sendable(&value);
}


template<class T>
inline Send_channel<T>&
Send_channel<T>::operator=(Send_channel other)
{
    pimpl = std::move(other.pimpl);
    return *this;
}


template<class T>
inline Send_channel<T>&
Send_channel<T>::operator=(const Channel<T>& other)
{
    pimpl = other.pimpl;
    return *this;
}


template<class T>
inline
Send_channel<T>::operator bool() const
{
    return pimpl;
}


template<class T>
inline typename Send_channel<T>::Awaitable
Send_channel<T>::send(const T& value) const
{
    return pimpl->awaitable_send(&value);
}


template<class T>
inline typename Send_channel<T>::Awaitable
Send_channel<T>::send(T&& value) const
{
    return pimpl->awaitable_send(&value);
}


template<class T>
inline Channel_size
Send_channel<T>::size() const
{
    return pimpl->size();
}


template<class T>
inline void
Send_channel<T>::sync_send(const T& value) const
{
    pimpl->sync_send(value);
}


template<class T>
inline void
Send_channel<T>::sync_send(T&& value) const
{
    using std::move;
    pimpl->sync_send(move(value));
}


template<class T>
inline bool
Send_channel<T>::try_send(const T& value) const
{
    return pimpl->try_send(value);
}


template<class T>
inline bool
operator==(const Send_channel<T>& x, const Send_channel<T>& y)
{
    return x.pimpl == y.pimpl;
}


template<class T>
inline bool
operator< (const Send_channel<T>& x, const Send_channel<T>& y)
{
    return x.pimpl < y.pimpl;
}


template<class T>
inline void
swap(Send_channel<T>& x, Send_channel<T>& y)
{
    using std::swap;
    swap(x.pimpl, y.pimpl);
}


/*
    Implementation Details
*/
namespace Detail {


/*
    Names/Types
*/
using std::move;
    
    
/*
    Channel Alternatives
*/
void
Channel_alternatives::enqueue(Goroutine::Handle g)
{
    for (Channel_operation* op = first; op != last; ++op)
        op->enqueue(g);
}


void
Channel_alternatives::lock(Channel_operation* first, Channel_operation* last)
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
    
   
Channel_size
Channel_alternatives::random_ready(Channel_size max)
{
    using Device        = std::random_device;
    using Engine        = std::default_random_engine;
    using Distribution  = std::uniform_int_distribution<Channel_size>;

    Device          rand;
    Engine          engine(rand());
    Distribution    dist(0, max);

    return dist(engine);
}
    
   
void
Channel_alternatives::restore_positions(Channel_operation* first, Channel_operation* last)
{
    using std::sort;

    sort(first, last, position_less());
}


void
Channel_alternatives::save_positions(Channel_operation* first, Channel_operation* last)
{
    int i = 0;

    for (Channel_operation* p = first; p != last; ++p)
        p->pos = ++i;
}


template<Channel_size N>
optional<Channel_size>
Channel_alternatives::select(Channel_operation (&ops)[N])
{
    using std::sort;

    Channel_operation* first    = begin(ops);
    Channel_operation* last     = end(ops);

    save_positions(first, last);
    sort(first, last);
    lock(first, last);

    Channel_operation* p = select(first, last);
    if (p != last)
        selectpos = p->pos;

    unlock(first, last);
    restore_positions(first, last);

    return selectpos;
}


Channel_operation*
Channel_alternatives::select(Channel_operation* first, Channel_operation* last)
{
    Channel_operation*  selectp = last;
    Channel_size        nready  = 0;

    for (Channel_operation* op = first; op != last; ++op) {
        if (op->is_ready())
            ++nready;
    }

    Channel_size n = random_ready(nready);

    for (Channel_operation* op = first; op != last; ++op) {
        if (op->is_ready() && n-- == 0) {
            op->execute();
            selectp = op;
            break;
        }
    }

    return selectp;
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
}
    
   
/*
    Channel Buffer
*/
template<class T>
inline
Channel_buffer<T>::Channel_buffer(Channel_size maxsize)
    : sizemax(maxsize >=0 ? maxsize : 0)
{
    assert(maxsize >= 0);
}


template<class T>
inline bool
Channel_buffer<T>::is_empty() const
{
    return q.empty();
}


template<class T>
inline bool
Channel_buffer<T>::is_full() const
{
    return size() == max_size();
}


template<class T>
inline Channel_size
Channel_buffer<T>::max_size() const
{
    return sizemax;
}


template<class T>
inline void
Channel_buffer<T>::pop(T* valuep)
{
    using std::move;

    *valuep = move(q.front());
    q.pop();
}


template<class T>
inline void
Channel_buffer<T>::pop(optional<T>* valuep)
{
    using std::move;

    *valuep = move(q.front());
    q.pop();
}


template<class T>
template<class U>
inline void
Channel_buffer<T>::push(U&& value)
{
    using std::move;

    q.push(move(value));
}


template<class T>
inline Channel_size
Channel_buffer<T>::size() const
{
    return static_cast<Channel_size>(q.size());
}


/*
    Wait Queue
*/
template<class T>
inline bool
Wait_queue<T>::is_empty() const
{
    return ws.empty();
}


template<class T>
inline bool
Wait_queue<T>::find(const Waiter& w) const
{
    using std::find;

    const auto p = find(ws.begin(), ws.end(), w);
    return p != ws.end();
}


template<class T>
inline T
Wait_queue<T>::pop()
{
    const T w = ws.front();

    ws.pop_front();
    return w;
}


template<class T>
inline void
Wait_queue<T>::push(const Waiter& w)
{
    ws.push_back(w);
}


/*
    Waiting Receiver
*/
template<class T>
inline
Waiting_receiver<T>::Waiting_receiver(Goroutine::Handle receiver, T* valuep)
    : g{receiver}
    , readyp{nullptr}
    , valp{valuep}
{
    assert(receiver);
    assert(valuep != nullptr);
}


template<class T>
inline
Waiting_receiver<T>::Waiting_receiver(condition_variable* sysreadyp, T* valuep)
    : readyp{sysreadyp}
    , valp{valuep}
{
    assert(sysreadyp != nullptr);
    assert(valuep != nullptr);
}


template<class T>
inline Goroutine::Handle
Waiting_receiver<T>::goroutine() const
{
    return g;
}


template<class T>
template<class U>
inline void
Waiting_receiver<T>::resume(U* valuep) const
{
    *valp = move(*valuep);

    if (g)
        scheduler.resume(g);
    else
        readyp->notify_one();
}


template<class T>
inline condition_variable*
Waiting_receiver<T>::system_signal() const
{
    return readyp;
}


template<class T>
inline bool
operator==(const Waiting_receiver<T>& x, const Waiting_receiver<T>& y)
{
    if (x.g != y.g) return false;
    if (x.readyp != y.readyp) return false;
    if (x.valp != y.valp) return false;
    return true;
}


/*
    Waiting Sender
*/
template<class T>
inline
Waiting_sender<T>::Waiting_sender(Goroutine::Handle sender, const T* rvaluep)
    : g{sender}
    , readyp{nullptr}
    , rvalp{rvaluep}
    , lvalp{nullptr}
{
    assert(sender);
    assert(rvaluep != nullptr);
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(Goroutine::Handle sender, T* lvaluep)
    : g{sender}
    , readyp{nullptr}
    , rvalp{nullptr}
    , lvalp{lvaluep}
{
    assert(sender);
    assert(lvaluep != nullptr);
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(condition_variable* sysreadyp, const T* rvaluep)
    : readyp{sysreadyp}
    , rvalp{rvaluep}
    , lvalp{nullptr}
{
    assert(sysreadyp != nullptr);
    assert(rvaluep != nullptr);
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(condition_variable* sysreadyp, T* lvaluep)
    : readyp{sysreadyp}
    , rvalp{nullptr}
    , lvalp{lvaluep}
{
    assert(sysreadyp != nullptr);
    assert(lvaluep != nullptr);
}


template<class T>
inline Goroutine::Handle
Waiting_sender<T>::goroutine() const
{
    return g;
}


template<class T>
inline void
Waiting_sender<T>::resume(T* valuep) const
{
    *valuep = lvalp ? move(*lvalp) : *rvalp;

    if (g)
        scheduler.resume(g);
    else
        readyp->notify_one();
}


template<class T>
inline void
Waiting_sender<T>::resume(Channel_buffer<T>* bufp) const
{
    if (lvalp)
        bufp->push(move(*lvalp));
    else
        bufp->push(*rvalp);

    if (g)
        scheduler.resume(g);
    else
        readyp->notify_one();
}


template<class T>
inline condition_variable*
Waiting_sender<T>::system_signal() const
{
    return readyp;
}


template<class T>
inline bool
operator==(const Waiting_sender<T>& x, const Waiting_sender<T>& y)
{
    if (x.g != y.g) return false;
    if (x.readyp != y.readyp) return false;
    if (x.rvalp != y.rvalp) return false;
    if (x.lvalp != y.rvalp) return false;
    return true;
}


}   // Implementation Details


/*
    Channel Select Awaitable
*/
template<Channel_size N>
Channel_select_awaitable::Channel_select_awaitable(Channel_operation (&ops)[N])
{
    alternatives.select(ops);
}


inline bool
Channel_select_awaitable::await_ready()
{
    return false;
}


inline optional<Channel_size>
Channel_select_awaitable::await_resume()
{
    return alternatives.selected();
}


inline bool
Channel_select_awaitable::await_suspend(Goroutine::Handle g)
{
    bool is_suspended = false;

    if (alternatives.selected()) {
        alternatives.enqueue(g);
        scheduler.suspend(g);
        is_suspended = true;
    }

    return is_suspended;
}


/*
    Channel
*/
template<class T>
Channel<T>
make_channel(Channel_size n)
{
    using std::make_shared;

    return make_shared<Channel<T>::Impl>(n);
}


/*
    Goroutine Launcher
*/
template<class GoFun, class... Args>
inline void
go(GoFun&& fun, Args&&... args)
{
    using std::forward;
    using std::move;

    Goroutine g = fun(forward<Args>(args)...);
    scheduler.submit(move(g));
}


}  // Concurrency
}  // Isptech

