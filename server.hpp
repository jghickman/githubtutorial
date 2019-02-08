//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/server.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/server.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_SERVER_HPP
#define ISPTECH_ORB_SERVER_HPP

#include "isptech/orb/object.hpp"
#include "isptech/orb/string.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


class Object_ref {
public:
    // Observers
    Object_id identity() const;
};


class Object_set {
public:
    Object_ref  add(Object) const;
    Object      remove(Object_id) const;
    Object_ref  find(Object_id) const;
};


/*
    Server
*/
class Server : boost::totally_ordered<Server> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Server(Interface_ptr = Interface_ptr());
    Server(const Server&) = default;
    Server& operator=(const Server&) = default;
    Server(Server&&);
    Server& operator=(Server&&);
    friend void swap(Server&, Server&);

    // Observers
    const string& name() const;

    // Objects
    void    add(Object) const;
    Object  remove(Object_id) const;

    // Execution
    void start() const;
    void stop() const;

    // Conversions
    operator bool() const;

    // Comparisons
    friend bool operator==(const Server&, const Server&);
    friend bool operator< (const Server&, const Server&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Server Interface
*/
class Server::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Observers
    virtual const string& name() const = 0;

    // Execution
    virtual void start() = 0;
    virtual void stop() = 0;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_SERVER_HPP

//  $CUSTOM_FOOTER$
