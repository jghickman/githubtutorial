//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/object_adapter.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/object_adapter.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_OBJECT_ADAPTER_HPP
#define ISPTECH_ORB_OBJECT_ADAPTER_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/object.hpp"
#include "isptech/coroutine/task.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Object Adapter
*/
class Object_adapter : boost::totally_ordered<Object_adapter> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Object_adapter() = default;
    Object_adapter(Object_id, Interface_ptr);
    Object_adapter(const Object_adapter&) = default;
    Object_adapter& operator=(const Object_adapter&) = default;
    Object_adapter(Object_adapter&&);
    Object_adapter& operator=(Object_adapter&&);
    friend void swap(Object_adapter&, Object_adapter&);

    // Execution
    void start() const;
    void stop() const;

    // Comparisons
    friend bool operator==(const Object_adapter&, const Object_adapter&);
    friend bool operator< (const Object_adapter&, const Object_adapter&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Object Adapter Interface
*/
class Object_adapter::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Execution
    virtual void start() const = 0;
    virtual void stop() const = 0;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_OBJECT_ADAPTER_HPP

//  $CUSTOM_FOOTER$
