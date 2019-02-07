//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/client.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/client.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_CLIENT_HPP
#define ISPTECH_ORB_CLIENT_HPP

#include "isptech/orb/proxy.hpp"
#include "isptech/orb/string.hpp"
#include "boost/operators.hpp"
#include <memory>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Client
*/
class Client : boost::totally_ordered<Client> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Client(Interface_ptr = Interface_ptr());
    Client(const Client&) = default;
    Client& operator=(const Client&) = default;
    Client(Client&&);
    Client& operator=(Client&&);
    friend void swap(Client&, Client&);

    // Observers
    const string& name() const;

    // Proxy Construction
    Twoway_proxy make_twoway_proxy(Object_id) const;
    Twoway_proxy make_twoway_proxy(Object_id, const string& network, const string& address) const;

    // Conversions
    operator bool() const;

    // Comparisons
    friend bool operator==(const Client&, const Client&);
    friend bool operator< (const Client&, const Client&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Client Interface
*/
class Client::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Observers
    virtual const string& name() const = 0;

    // Proxy Construction
    virtual Twoway_proxy make_twoway(Object_id, const string& network, const string& address);
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_CLIENT_HPP

//  $CUSTOM_FOOTER$
#pragma once
