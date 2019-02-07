//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/broker.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/broker.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_BROKER_HPP
#define ISPTECH_ORB_BROKER_HPP

#include "isptech/orb/client.hpp"
#include "isptech/orb/server.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Broker
*/
class Broker : boost::totally_ordered<Broker> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Broker(Interface_ptr = Interface_ptr());
    Broker(const Broker&) = default;
    Broker& operator=(const Broker&) = default;
    Broker(Broker&&);
    Broker& operator=(Broker&&);
    friend void swap(Broker&, Broker&);

    // Client/Server Access
    Client make_client() const;
    Server make_server(const Server_name&) const;

    // Conversions
    operator bool() const;

    // Comparisons
    friend bool operator==(const Broker&, const Broker&);
    friend bool operator< (const Broker&, const Broker&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Broker Interface
*/
class Broker::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Client/Server Construction
    virtual Server make_server(const Server_name&) = 0;

    // Execution
    virtual void start() const = 0;
    virtual void stop() const = 0;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_BROKER_HPP

//  $CUSTOM_FOOTER$
