/* Copyright (c) 2006-2015. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef SIMGRID_S4U_MAILBOX_HPP
#define SIMGRID_S4U_MAILBOX_HPP

#include <boost/unordered_map.hpp>

#include <xbt/base.h>
#include <simgrid/s4u/forward.hpp>
#include <simgrid/s4u/actor.hpp>

namespace simgrid {
namespace s4u {

/** @brief Mailboxes
 *
 * Rendez-vous point for network communications, similar to URLs on which you could post and retrieve data.
 * They are not network locations: you can post and retrieve on a given mailbox from anywhere on the network.
 * You can access any mailbox without any latency. The network delay are only related to the location of the
 * sender and receiver.
 */
XBT_PUBLIC_CLASS Mailbox {
  friend Comm;

private:
  Mailbox(const char*name, smx_rdv_t inferior);
public:
  ~Mailbox();
  
protected:
  smx_rdv_t getInferior() { return p_inferior; }

public:
  /** Get the name of that mailbox */
  const char *getName();
  /** Retrieve the mailbox associated to the given string */
  static Mailbox *byName(const char *name);

private:
  std::string p_name;
  smx_rdv_t p_inferior;
  static boost::unordered_map<std::string, Mailbox *> *mailboxes;
};
}} // namespace simgrid::s4u

#endif /* SIMGRID_S4U_MAILBOX_HPP */
