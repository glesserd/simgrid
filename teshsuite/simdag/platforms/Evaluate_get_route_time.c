/* Copyright (c) 2008-2015. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

//for i in $(seq 1 100); do teshsuite/simdag/platforms/evaluate_get_route_time ../examples/platforms/cluster.xml 1 2> /tmp/null ; done


#include <stdio.h>
#include <stdlib.h>
#include "simgrid/simdag.h"
#include "surf/surf.h"
#include "xbt/xbt_os_time.h"

#define BILLION  1000000000L;

int main(int argc, char **argv)
{
  sg_host_t w1, w2;
  const sg_host_t *workstations;
  int i, j;
  int list_size;
  xbt_os_timer_t timer = xbt_os_timer_new();

  /* SD initialization */
  SD_init(&argc, argv);

  /* creation of the environment */
  SD_create_environment(argv[1]);

  workstations = sg_host_list();
  list_size = sg_host_count();

  /* Random number initialization */
  srand( (int) (xbt_os_time()*1000) );

  do{
    i = rand()%list_size;
    j = rand()%list_size;
  }while(i==j);

  w1 = workstations[i];
  w2 = workstations[j];
  printf("%d\tand\t%d\t\t",i,j);

  xbt_os_cputimer_start(timer);
  SD_route_get_list(w1, w2);
  xbt_os_cputimer_stop(timer);

  printf("%f\n", xbt_os_timer_elapsed(timer) );

  SD_exit();

  return 0;
}
