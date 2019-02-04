//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/buffer.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/buffer.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_BUFFER_HPP
#define ISPTECH_ORB_BUFFER_HPP

#include "boost/operators.hpp"
#include <cstddef>
#include <cstring>
#include <vector>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Names/Types
*/
using Buffer_size = std::ptrdiff_t;


/*
    Constant Buffer
*/
class Const_buffer : boost::totally_ordered<Const_buffer> {
public:
    // Construct
    Const_buffer() = default;
    Const_buffer(const void* start, Buffer_size length);

    // Modifiers
    void data(const void* start);
    void size(Buffer_size length);
    void reset(const void* start, Buffer_size length);

    // Observers
    const void* data() const;
    Buffer_size size() const;

    // Comparisons
    friend bool operator==(Const_buffer, Const_buffer);
    friend bool operator< (Const_buffer, Const_buffer);

private:
    // Data
    const void* bufp;
    Buffer_size nbytes;
};


class Const_buffers {
public:
    // Construct
    Const_buffers() = default;
    Const_buffers(const Const_buffer*, Buffer_size length);

    // Iterators
    const Const_buffer* begin() const;
    const Const_buffer* end() const;

    // Observers
    Buffer_size size() const;

    // Modifiers
    void size(Buffer_size length);
    void reset(const Const_buffer* start, Buffer_size length);

    // Comparisons
    friend bool operator==(Const_buffers, Const_buffers);
    friend bool operator< (Const_buffers, Const_buffers);

private:
    // Data
    const Const_buffer* bufp;
    Buffer_size         nbufs;
};


/*
    Mutable Buffer
*/
class Mutable_buffer : boost::totally_ordered<Mutable_buffer> {
public:
    // Construct
    Mutable_buffer() = default;
    Mutable_buffer(void* start, Buffer_size length);

    // Modifiers
    void data(void* start);
    void size(Buffer_size length);
    void reset(void* start, Buffer_size length);

    // Observers
    void*       data() const;
    Buffer_size size() const;

    // Conversions
    operator Const_buffer() const;

    // Comparisons
    friend bool operator==(Mutable_buffer, Mutable_buffer);
    friend bool operator< (Mutable_buffer, Mutable_buffer);

private:
    // Data
    void*       bufp;
    Buffer_size nbytes;
};


/*
    Mutable Data Buffers
*/
class Mutable_buffers : boost::totally_ordered<Mutable_buffers> {
public:
    // Construct
    Mutable_buffers() = default;
    Mutable_buffers(const Mutable_buffer* start, Buffer_size length);

    // Modifiers
    void data(const Mutable_buffer* start);
    void size(Buffer_size length);
    void reset(const Mutable_buffer* start, Buffer_size length);

    // Observers
    const Mutable_buffer*   data() const;
    Buffer_size size()      const;

    // Comparisons
    friend bool operator==(Mutable_buffers, Mutable_buffers);
    friend bool operator< (Mutable_buffers, Mutable_buffers);

private:
    // Data
    const Mutable_buffer*   bufp;
    Buffer_size             nbufs;
};


/*
    I/O Buffer
*/
class Io_buffer : boost::totally_ordered<Io_buffer> {
public:
    // Construct/Move/Copy
    Io_buffer();
    Io_buffer(const void*, Buffer_size);
    Io_buffer(const Io_buffer&) = default;
    Io_buffer& operator=(const Io_buffer&) = default;
    Io_buffer(Io_buffer&&);
    Io_buffer& operator=(Io_buffer&&);
    friend void swap(Io_buffer&, Io_buffer&);

    // Modifiers
    void write(const void*, Buffer_size);
    void read(void*, Buffer_size);

    // Capacity
    void        reserve(Buffer_size);
    Buffer_size capacity() const;

    // Conversions
    operator Mutable_buffer();
    operator Const_buffer() const;

    // Comparisons
    friend bool operator==(Io_buffer, Io_buffer);
    friend bool operator< (Io_buffer, Io_buffer);

private:
    // Data
    std::vector<unsigned char>  bytes;
    Buffer_size                 nextput;
    Buffer_size                 nextget;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_BUFFER_HPP

//  $CUSTOM_FOOTER$
