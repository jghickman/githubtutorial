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
