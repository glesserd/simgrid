/* Copyright (c) 2006-2015. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef SIMGRID_S4U_ENGINE_HPP
#define SIMGRID_S4U_ENGINE_HPP

#include <xbt/base.h>

namespace simgrid {
namespace s4u {
/** @brief Simulation engine
 *
 * This class is an interface to the simulation engine.
 */
XBT_PUBLIC_CLASS Engine {
public:
	/** Constructor, taking the command line parameters of your main function */
	Engine(int *argc, char **argv);

	/** @brief Load a platform file describing the environment
	 *
	 * The environment is either a XML file following the simgrid.dtd formalism, or a lua file.
	 * Some examples can be found in the directory examples/platforms.
	 */
	void loadPlatform(const char *platf);

	/** Registers the main function of an actor that will be launched from the deployment file */
	void register_function(const char*name, int (*code)(int,char**));

	/** Registers a function as the default main function of actors
	 *
	 * It will be used as fallback when the function requested from the deployment file was not registered.
	 * It is used for trace-based simulations (see examples/msg/actions).
	 */
	void register_default(int (*code)(int,char**));

	/** @brief Load a deployment file and launch the actors that it contains */
	void loadDeployment(const char *deploy);

	/** @brief Run the simulation */
	void run();

	/** @brief Retrieve the simulation time */
	static double getClock();
	
};
}} // namespace simgrid::sgo

#endif /* SIMGRID_S4U_ENGINE_HPP */