/* Copyright (c) 2013-2015. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "cpu_ti.hpp"
#include "xbt/heap.h"
#include "src/surf/trace_mgr.hpp"
#include "src/surf/platform.hpp"

#ifndef SURF_MODEL_CPUTI_H_
#define SURF_MODEL_CPUTI_H_

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(surf_cpu_ti, surf_cpu,
                                "Logging specific to the SURF CPU TRACE INTEGRATION module");

namespace simgrid {
namespace surf {
  
static inline
void cpu_ti_action_update_index_heap(void *action, int i)
{
  ((simgrid::surf::CpuTiAction*)action)->updateIndexHeap(i);
}

/*********
 * Trace *
 *********/

CpuTiTrace::CpuTiTrace(tmgr_trace_t speedTrace)
{
  s_tmgr_event_t val;
  unsigned int cpt;
  double integral = 0;
  double time = 0;
  int i = 0;
  p_timePoints = (double*) xbt_malloc0(sizeof(double) *
                  (xbt_dynar_length(speedTrace->event_list) + 1));
  p_integral = (double*) xbt_malloc0(sizeof(double) *
                  (xbt_dynar_length(speedTrace->event_list) + 1));
  m_nbPoints = xbt_dynar_length(speedTrace->event_list) + 1;
  xbt_dynar_foreach(speedTrace->event_list, cpt, val) {
    p_timePoints[i] = time;
    p_integral[i] = integral;
    integral += val.delta * val.value;
    time += val.delta;
    i++;
  }
  p_timePoints[i] = time;
  p_integral[i] = integral;
}

CpuTiTrace::~CpuTiTrace()
{
  xbt_free(p_timePoints);
  xbt_free(p_integral);
}

CpuTiTgmr::~CpuTiTgmr()
{
  if (p_trace)
    delete p_trace;
}

/**
* \brief Integrate trace
*
* Wrapper around surf_cpu_integrate_trace_simple() to get
* the cyclic effect.
*
* \param trace Trace structure.
* \param a      Begin of interval
* \param b      End of interval
* \return the integrate value. -1 if an error occurs.
*/
double CpuTiTgmr::integrate(double a, double b)
{
  double first_chunk;
  double middle_chunk;
  double last_chunk;
  int a_index, b_index;

  if ((a < 0.0) || (a > b)) {
    XBT_CRITICAL
        ("Error, invalid integration interval [%.2f,%.2f]. You probably have a task executing with negative computation amount. Check your code.",
         a, b);
    xbt_abort();
  }
  if (a == b)
    return 0.0;

  if (m_type == TRACE_FIXED) {
    return ((b - a) * m_value);
  }

  if (ceil(a / m_lastTime) == a / m_lastTime)
    a_index = 1 + (int) (ceil(a / m_lastTime));
  else
    a_index = (int) (ceil(a / m_lastTime));

  b_index = (int) (floor(b / m_lastTime));

  if (a_index > b_index) {      /* Same chunk */
    return p_trace->integrateSimple(a - (a_index -
                                              1) * m_lastTime,
                                         b -
                                         (b_index) *
                                         m_lastTime);
  }

  first_chunk = p_trace->integrateSimple(a - (a_index -
                                                   1) *
                                              m_lastTime,
                                              m_lastTime);
  middle_chunk = (b_index - a_index) * m_total;
  last_chunk = p_trace->integrateSimple(0.0,
                                             b -
                                             (b_index) *
                                             m_lastTime);

  XBT_DEBUG("first_chunk=%.2f  middle_chunk=%.2f  last_chunk=%.2f\n",
         first_chunk, middle_chunk, last_chunk);

  return (first_chunk + middle_chunk + last_chunk);
}

/**
 * \brief Auxiliary function to compute the integral between a and b.
 *     It simply computes the integrals at point a and b and returns the difference between them.
 * \param trace    Trace structure
 * \param a        Initial point
 * \param b  Final point
 * \return  Integral
*/
double CpuTiTrace::integrateSimple(double a, double b)
{
  return integrateSimplePoint(b) - integrateSimplePoint(a);
}

/**
 * \brief Auxiliary function to compute the integral at point a.
 * \param trace    Trace structure
 * \param a        point
 * \return  Integral
*/
double CpuTiTrace::integrateSimplePoint(double a)
{
  double integral = 0;
  int ind;
  double a_aux = a;
  ind = binarySearch(p_timePoints, a, 0, m_nbPoints - 1);
  integral += p_integral[ind];
  XBT_DEBUG
      ("a %f ind %d integral %f ind + 1 %f ind %f time +1 %f time %f",
       a, ind, integral, p_integral[ind + 1], p_integral[ind],
       p_timePoints[ind + 1], p_timePoints[ind]);
  double_update(&a_aux, p_timePoints[ind], sg_maxmin_precision*sg_surf_precision);
  if (a_aux > 0)
    integral +=
        ((p_integral[ind + 1] -
          p_integral[ind]) / (p_timePoints[ind + 1] -
                              p_timePoints[ind])) * (a - p_timePoints[ind]);
  XBT_DEBUG("Integral a %f = %f", a, integral);

  return integral;
}

/**
* \brief Computes the time needed to execute "amount" on cpu.
*
* Here, amount can span multiple trace periods
*
* \param trace   CPU trace structure
* \param a        Initial time
* \param amount  Amount to be executed
* \return  End time
*/
double CpuTiTgmr::solve(double a, double amount)
{
  int quotient;
  double reduced_b;
  double reduced_amount;
  double reduced_a;
  double b;

/* Fix very small negative numbers */
  if ((a < 0.0) && (a > -EPSILON)) {
    a = 0.0;
  }
  if ((amount < 0.0) && (amount > -EPSILON)) {
    amount = 0.0;
  }

/* Sanity checks */
  if ((a < 0.0) || (amount < 0.0)) {
    XBT_CRITICAL
        ("Error, invalid parameters [a = %.2f, amount = %.2f]. You probably have a task executing with negative computation amount. Check your code.",
         a, amount);
    xbt_abort();
  }

/* At this point, a and amount are positive */

  if (amount < EPSILON)
    return a;

/* Is the trace fixed ? */
  if (m_type == TRACE_FIXED) {
    return (a + (amount / m_value));
  }

  XBT_DEBUG("amount %f total %f", amount, m_total);
/* Reduce the problem to one where amount <= trace_total */
  quotient = (int) (floor(amount / m_total));
  reduced_amount = (m_total) * ((amount / m_total) -
                                     floor(amount / m_total));
  reduced_a = a - (m_lastTime) * (int) (floor(a / m_lastTime));

  XBT_DEBUG("Quotient: %d reduced_amount: %f reduced_a: %f", quotient,
         reduced_amount, reduced_a);

/* Now solve for new_amount which is <= trace_total */
/*
   fprintf(stderr,"reduced_a = %.2f\n",reduced_a);
   fprintf(stderr,"reduced_amount = %.2f\n",reduced_amount);
 */
  reduced_b = solveSomewhatSimple(reduced_a, reduced_amount);

/* Re-map to the original b and amount */
  b = (m_lastTime) * (int) (floor(a / m_lastTime)) +
      (quotient * m_lastTime) + reduced_b;
  return b;
}

/**
* \brief Auxiliary function to solve integral
*
* Here, amount is <= trace->total
* and a <=trace->last_time
*
*/
double CpuTiTgmr::solveSomewhatSimple(double a, double amount)
{
  double amount_till_end;
  double b;

  XBT_DEBUG("Solve integral: [%.2f, amount=%.2f]", a, amount);
  amount_till_end = integrate(a, m_lastTime);
/*
   fprintf(stderr,"amount_till_end=%.2f\n",amount_till_end);
 */

  if (amount_till_end > amount) {
    b = p_trace->solveSimple(a, amount);
  } else {
    b = m_lastTime + p_trace->solveSimple(0.0, amount - amount_till_end);
  }
  return b;
}

/**
 * \brief Auxiliary function to solve integral.
 *  It returns the date when the requested amount of flops is available
 * \param trace    Trace structure
 * \param a        Initial point
 * \param amount  Amount of flops
 * \return The date when amount is available.
*/
double CpuTiTrace::solveSimple(double a, double amount)
{
  double integral_a;
  int ind;
  double time;
  integral_a = integrateSimplePoint(a);
  ind = binarySearch(p_integral, integral_a + amount, 0, m_nbPoints - 1);
  time = p_timePoints[ind];
  time +=
      (integral_a + amount -
       p_integral[ind]) / ((p_integral[ind + 1] -
                                 p_integral[ind]) /
                                (p_timePoints[ind + 1] -
                                 p_timePoints[ind]));

  return time;
}

/**
* \brief Auxiliary function to update the CPU speed scale.
*
*  This function uses the trace structure to return the speed scale at the determined time a.
* \param trace    Trace structure to search the updated speed scale
* \param a        Time
* \return CPU speed scale
*/
double CpuTiTgmr::getPowerScale(double a)
{
  double reduced_a;
  int point;
  s_tmgr_event_t val;

  reduced_a = a - floor(a / m_lastTime) * m_lastTime;
  point = p_trace->binarySearch(p_trace->p_timePoints, reduced_a, 0,
                                p_trace->m_nbPoints - 1);
  xbt_dynar_get_cpy(p_speedTrace->event_list, point, &val);
  return val.value;
}

/**
* \brief Creates a new integration trace from a tmgr_trace_t
*
* \param  speedTrace    CPU availability trace
* \param  value          Percentage of CPU speed available (useful to fixed tracing)
* \param  spacing        Initial spacing
* \return  Integration trace structure
*/
CpuTiTgmr::CpuTiTgmr(tmgr_trace_t speedTrace, double value)
{
  double total_time = 0.0;
  s_tmgr_event_t val;
  unsigned int cpt;
  p_trace = 0;

/* no availability file, fixed trace */
  if (!speedTrace) {
    m_type = TRACE_FIXED;
    m_value = value;
    XBT_DEBUG("No availability trace. Constant value = %f", value);
    return;
  }

  /* only one point available, fixed trace */
  if (xbt_dynar_length(speedTrace->event_list) == 1) {
    xbt_dynar_get_cpy(speedTrace->event_list, 0, &val);
    m_type = TRACE_FIXED;
    m_value = val.value;
    return;
  }

  m_type = TRACE_DYNAMIC;
  p_speedTrace = speedTrace;

  /* count the total time of trace file */
  xbt_dynar_foreach(speedTrace->event_list, cpt, val) {
    total_time += val.delta;
  }
  p_trace = new CpuTiTrace(speedTrace);
  m_lastTime = total_time;
  m_total = p_trace->integrateSimple(0, total_time);

  XBT_DEBUG("Total integral %f, last_time %f ",
            m_total, m_lastTime);
}

/**
 * \brief Binary search in array.
 *  It returns the first point of the interval in which "a" is.
 * \param array    Array
 * \param a        Value to search
 * \param low     Low bound to search in array
 * \param high    Upper bound to search in array
 * \return Index of point
*/
int CpuTiTrace::binarySearch(double *array, double a, int low, int high)
{
  xbt_assert(low < high, "Wrong parameters: low (%d) should be smaller than"
      " high (%d)", low, high);

  int mid;
  do {
    mid = low + (high - low) / 2;
    XBT_DEBUG("a %f low %d high %d mid %d value %f", a, low, high, mid,
        array[mid]);

    if (array[mid] > a)
      high = mid;
    else
      low = mid;
  }
  while (low < high - 1);

  return low;
}

}
}

/*********
 * Model *
 *********/

void surf_cpu_model_init_ti()
{
  xbt_assert(!surf_cpu_model_pm,"CPU model already initialized. This should not happen.");
  xbt_assert(!surf_cpu_model_vm,"CPU model already initialized. This should not happen.");

  surf_cpu_model_pm = new simgrid::surf::CpuTiModel();
  xbt_dynar_push(all_existing_models, &surf_cpu_model_pm);

  surf_cpu_model_vm = new simgrid::surf::CpuTiModel();
  xbt_dynar_push(all_existing_models, &surf_cpu_model_vm);
}

namespace simgrid {
namespace surf {

CpuTiModel::CpuTiModel() : CpuModel()
{
  p_runningActionSetThatDoesNotNeedBeingChecked = new ActionList();

  p_modifiedCpu = new CpuTiList();

  p_tiActionHeap = xbt_heap_new(8, NULL);
  xbt_heap_set_update_callback(p_tiActionHeap,
                               cpu_ti_action_update_index_heap);
}

CpuTiModel::~CpuTiModel()
{
  surf_cpu_model_pm = NULL;
  delete p_runningActionSetThatDoesNotNeedBeingChecked;
  delete p_modifiedCpu;
  xbt_heap_free(p_tiActionHeap);
}

Cpu *CpuTiModel::createCpu(simgrid::s4u::Host *host,
                         xbt_dynar_t speedPeak,
                         int pstate,
                           double speedScale,
                           tmgr_trace_t speedTrace,
                           int core,
                           int initiallyOn,
                           tmgr_trace_t stateTrace)
{
  xbt_assert(core==1,"Multi-core not handled with this model yet");
  xbt_assert(xbt_dynar_getfirst_as(speedPeak, double) > 0.0,
      "Speed has to be >0.0. Did you forget to specify the mandatory speed attribute?");
  CpuTi *cpu = new CpuTi(this, host, speedPeak, pstate, speedScale, speedTrace,
               core, initiallyOn, stateTrace);
  return cpu;
}

double CpuTiModel::next_occuring_event(double now)
{
  double min_action_duration = -1;

/* iterates over modified cpus to update share resources */
  for(CpuTiList::iterator it(p_modifiedCpu->begin()), itend(p_modifiedCpu->end())
     ; it != itend ;) {
    CpuTi *ti = &*it;
    ++it;
    ti->updateActionsFinishTime(now);
  }

/* get the min next event if heap not empty */
  if (xbt_heap_size(p_tiActionHeap) > 0)
    min_action_duration = xbt_heap_maxkey(p_tiActionHeap) - now;

  XBT_DEBUG("Share resources, min next event date: %f", min_action_duration);

  return min_action_duration;
}

void CpuTiModel::updateActionsState(double now, double /*delta*/)
{
  while ((xbt_heap_size(p_tiActionHeap) > 0)
         && (xbt_heap_maxkey(p_tiActionHeap) <= now)) {
    CpuTiAction *action = (CpuTiAction*) xbt_heap_pop(p_tiActionHeap);
    XBT_DEBUG("Action %p: finish", action);
    action->finish();
    /* set the remains to 0 due to precision problems when updating the remaining amount */
    action->setRemains(0);
    action->setState(SURF_ACTION_DONE);
    /* update remaining amount of all actions */
    action->p_cpu->updateRemainingAmount(surf_get_clock());
  }
}

/************
 * Resource *
 ************/
CpuTi::CpuTi(CpuTiModel *model, simgrid::s4u::Host *host, xbt_dynar_t speedPeak,
        int pstate, double speedScale, tmgr_trace_t speedTrace, int core,
        int initiallyOn, tmgr_trace_t stateTrace)
  : Cpu(model, host, NULL, pstate, core, 0, speedScale, initiallyOn)
{
  xbt_assert(core==1,"Multi-core not handled by this model yet");
  m_core = core;

  p_speed.scale = speedScale;
  p_availTrace = new CpuTiTgmr(speedTrace, speedScale);

  p_actionSet = new ActionTiList();

  xbt_dynar_get_cpy(speedPeak, 0, &p_speed.peak);
  XBT_DEBUG("CPU create: peak=%f", p_speed.peak);

  if (stateTrace)
    p_stateEvent = future_evt_set->add_trace(stateTrace, 0.0, this);

  if (speedTrace && xbt_dynar_length(speedTrace->event_list) > 1) {
  s_tmgr_event_t val;
    // add a fake trace event if periodicity == 0
    xbt_dynar_get_cpy(speedTrace->event_list,
                      xbt_dynar_length(speedTrace->event_list) - 1, &val);
    if (val.delta == 0) {
      p_speed.event =
          future_evt_set->add_trace(tmgr_empty_trace_new(), p_availTrace->m_lastTime, this);
    }
  }
}

CpuTi::~CpuTi()
{
  modified(false);
  delete p_availTrace;
  delete p_actionSet;
}
void CpuTi::set_speed_trace(tmgr_trace_t trace)
{
  if (p_availTrace)
    delete p_availTrace;

  p_availTrace = new CpuTiTgmr(trace, p_speed.scale);

  /* add a fake trace event if periodicity == 0 */
  if (trace && xbt_dynar_length(trace->event_list) > 1) {
    s_tmgr_event_t val;
    xbt_dynar_get_cpy(trace->event_list, xbt_dynar_length(trace->event_list) - 1, &val);
    if (val.delta == 0)
      p_speed.event = future_evt_set->add_trace(tmgr_empty_trace_new(), 0.0, this);
  }
}

void CpuTi::apply_event(tmgr_trace_iterator_t event, double value)
{
  if (event == p_speed.event) {
    tmgr_trace_t speedTrace;
    CpuTiTgmr *trace;
    s_tmgr_event_t val;

    XBT_DEBUG("Finish trace date: value %f", value);
    /* update remaining of actions and put in modified cpu swag */
    updateRemainingAmount(surf_get_clock());

    modified(true);

    speedTrace = p_availTrace->p_speedTrace;
    xbt_dynar_get_cpy(speedTrace->event_list,
                      xbt_dynar_length(speedTrace->event_list) - 1, &val);
    /* free old trace */
    delete p_availTrace;
    p_speed.scale = val.value;

    trace = new CpuTiTgmr(TRACE_FIXED, val.value);
    XBT_DEBUG("value %f", val.value);

    p_availTrace = trace;

    tmgr_trace_event_unref(&p_speed.event);

  } else if (event == p_stateEvent) {
    if (value > 0) {
      if(isOff())
        xbt_dynar_push_as(host_that_restart, char*, (char *)getName());
      turnOn();
    } else {
      turnOff();
      double date = surf_get_clock();

      /* put all action running on cpu to failed */
      for(ActionTiList::iterator it(p_actionSet->begin()), itend(p_actionSet->end())
          ; it != itend ; ++it) {

        CpuTiAction *action = &*it;
        if (action->getState() == SURF_ACTION_RUNNING
         || action->getState() == SURF_ACTION_READY
         || action->getState() == SURF_ACTION_NOT_IN_THE_SYSTEM) {
          action->setFinishTime(date);
          action->setState(SURF_ACTION_FAILED);
          if (action->m_indexHeap >= 0) {
            CpuTiAction *heap_act = (CpuTiAction*)
                xbt_heap_remove(static_cast<CpuTiModel*>(getModel())->p_tiActionHeap, action->m_indexHeap);
            if (heap_act != action)
              DIE_IMPOSSIBLE;
          }
        }
      }
    }
    tmgr_trace_event_unref(&p_stateEvent);

  } else {
    xbt_die("Unknown event!\n");
  }
}

void CpuTi::updateActionsFinishTime(double now)
{
  CpuTiAction *action;
  double sum_priority = 0.0, total_area, min_finish = -1;

  /* update remaining amount of actions */
  updateRemainingAmount(now);

  for(ActionTiList::iterator it(p_actionSet->begin()), itend(p_actionSet->end())
      ; it != itend ; ++it) {
    action = &*it;
    /* action not running, skip it */
    if (action->getStateSet() !=
        surf_cpu_model_pm->getRunningActionSet())
      continue;

    /* bogus priority, skip it */
    if (action->getPriority() <= 0)
      continue;

    /* action suspended, skip it */
    if (action->m_suspended != 0)
      continue;

    sum_priority += 1.0 / action->getPriority();
  }
  m_sumPriority = sum_priority;

  for(ActionTiList::iterator it(p_actionSet->begin()), itend(p_actionSet->end())
     ; it != itend ; ++it) {
    action = &*it;
    min_finish = -1;
    /* action not running, skip it */
    if (action->getStateSet() !=
        surf_cpu_model_pm->getRunningActionSet())
      continue;

    /* verify if the action is really running on cpu */
    if (action->m_suspended == 0 && action->getPriority() > 0) {
      /* total area needed to finish the action. Used in trace integration */
      total_area =
          (action->getRemains()) * sum_priority *
           action->getPriority();

      total_area /= p_speed.peak;

      action->setFinishTime(p_availTrace->solve(now, total_area));
      /* verify which event will happen before (max_duration or finish time) */
      if (action->getMaxDuration() != NO_MAX_DURATION &&
          action->getStartTime() + action->getMaxDuration() < action->m_finish)
        min_finish = action->getStartTime() + action->getMaxDuration();
      else
        min_finish = action->m_finish;
    } else {
      /* put the max duration time on heap */
      if (action->getMaxDuration() != NO_MAX_DURATION)
        min_finish = action->getStartTime() + action->getMaxDuration();
    }
    /* add in action heap */
    XBT_DEBUG("action(%p) index %d", action, action->m_indexHeap);
    if (action->m_indexHeap >= 0) {
      CpuTiAction *heap_act = (CpuTiAction*)
          xbt_heap_remove(static_cast<CpuTiModel*>(getModel())->p_tiActionHeap, action->m_indexHeap);
      if (heap_act != action)
        DIE_IMPOSSIBLE;
    }
    if (min_finish != NO_MAX_DURATION)
      xbt_heap_push(static_cast<CpuTiModel*>(getModel())->p_tiActionHeap, action, min_finish);

    XBT_DEBUG
        ("Update finish time: Cpu(%s) Action: %p, Start Time: %f Finish Time: %f Max duration %f",
         getName(), action, action->getStartTime(),
         action->m_finish,
         action->getMaxDuration());
  }
  /* remove from modified cpu */
  modified(false);
}

bool CpuTi::isUsed()
{
  return !p_actionSet->empty();
}

double CpuTi::getAvailableSpeed()
{
  p_speed.scale = p_availTrace->getPowerScale(surf_get_clock());
  return Cpu::getAvailableSpeed();
}

/** @brief Update the remaining amount of actions */
void CpuTi::updateRemainingAmount(double now)
{
  double area_total;
  CpuTiAction *action;

  /* already updated */
  if (m_lastUpdate >= now)
    return;

  /* compute the integration area */
  area_total = p_availTrace->integrate(m_lastUpdate, now) * p_speed.peak;
  XBT_DEBUG("Flops total: %f, Last update %f", area_total,
         m_lastUpdate);

  for(ActionTiList::iterator it(p_actionSet->begin()), itend(p_actionSet->end())
     ; it != itend ; ++it) {
    action = &*it;
    /* action not running, skip it */
    if (action->getStateSet() !=
        getModel()->getRunningActionSet())
      continue;

    /* bogus priority, skip it */
    if (action->getPriority() <= 0)
      continue;

    /* action suspended, skip it */
    if (action->m_suspended != 0)
      continue;

    /* action don't need update */
    if (action->getStartTime() >= now)
      continue;

    /* skip action that are finishing now */
    if (action->m_finish >= 0
        && action->m_finish <= now)
      continue;

    /* update remaining */
    action->updateRemains(area_total / (m_sumPriority * action->getPriority()));
    XBT_DEBUG("Update remaining action(%p) remaining %f", action,
           action->m_remains);
  }
  m_lastUpdate = now;
}

CpuAction *CpuTi::execution_start(double size)
{
  XBT_IN("(%s,%g)", getName(), size);
  CpuTiAction *action = new CpuTiAction(static_cast<CpuTiModel*>(getModel()), size, isOff(), this);

  p_actionSet->push_back(*action);

  XBT_OUT();
  return action;
}


CpuAction *CpuTi::sleep(double duration)
{
  if (duration > 0)
    duration = MAX(duration, sg_surf_precision);

  XBT_IN("(%s,%g)", getName(), duration);
  CpuTiAction *action = new CpuTiAction(static_cast<CpuTiModel*>(getModel()), 1.0, isOff(), this);

  action->m_maxDuration = duration;
  action->m_suspended = 2;
  if (duration == NO_MAX_DURATION) {
   /* Move to the *end* of the corresponding action set. This convention
      is used to speed up update_resource_state  */
  action->getStateSet()->erase(action->getStateSet()->iterator_to(*action));
    action->p_stateSet = static_cast<CpuTiModel*>(getModel())->p_runningActionSetThatDoesNotNeedBeingChecked;
    action->getStateSet()->push_back(*action);
  }

  p_actionSet->push_back(*action);

  XBT_OUT();
  return action;
}

void CpuTi::modified(bool modified){
  CpuTiList *modifiedCpu = static_cast<CpuTiModel*>(getModel())->p_modifiedCpu;
  if (modified) {
    if (!cpu_ti_hook.is_linked()) {
      modifiedCpu->push_back(*this);
    }
  } else {
    if (cpu_ti_hook.is_linked()) {
      modifiedCpu->erase(modifiedCpu->iterator_to(*this));
    }
  }
}

/**********
 * Action *
 **********/

CpuTiAction::CpuTiAction(CpuTiModel *model_, double cost, bool failed,
                     CpuTi *cpu)
 : CpuAction(model_, cost, failed)
{
  p_cpu = cpu;
  m_indexHeap = -1;
  p_cpu->modified(true);
}

void CpuTiAction::updateIndexHeap(int i)
{
  m_indexHeap = i;
}

void CpuTiAction::setState(e_surf_action_state_t state)
{
  CpuAction::setState(state);
  p_cpu->modified(true);
}

int CpuTiAction::unref()
{
  m_refcount--;
  if (!m_refcount) {
    if (action_hook.is_linked())
      getStateSet()->erase(getStateSet()->iterator_to(*this));
    /* remove from action_set */
    if (action_ti_hook.is_linked())
      p_cpu->p_actionSet->erase(p_cpu->p_actionSet->iterator_to(*this));
    /* remove from heap */
    xbt_heap_remove(static_cast<CpuTiModel*>(getModel())->p_tiActionHeap, this->m_indexHeap);
    p_cpu->modified(true);
    delete this;
    return 1;
  }
  return 0;
}

void CpuTiAction::cancel()
{
  this->setState(SURF_ACTION_FAILED);
  xbt_heap_remove(getModel()->getActionHeap(), this->m_indexHeap);
  p_cpu->modified(true);
  return;
}

void CpuTiAction::suspend()
{
  XBT_IN("(%p)", this);
  if (m_suspended != 2) {
    m_suspended = 1;
    xbt_heap_remove(getModel()->getActionHeap(), m_indexHeap);
    p_cpu->modified(true);
  }
  XBT_OUT();
}

void CpuTiAction::resume()
{
  XBT_IN("(%p)", this);
  if (m_suspended != 2) {
    m_suspended = 0;
    p_cpu->modified(true);
  }
  XBT_OUT();
}

void CpuTiAction::setMaxDuration(double duration)
{
  double min_finish;

  XBT_IN("(%p,%g)", this, duration);

  m_maxDuration = duration;

  if (duration >= 0)
    min_finish = (getStartTime() + getMaxDuration()) < getFinishTime() ?
                 (getStartTime() + getMaxDuration()) : getFinishTime();
  else
    min_finish = getFinishTime();

/* add in action heap */
  if (m_indexHeap >= 0) {
    CpuTiAction *heap_act = (CpuTiAction*)
        xbt_heap_remove(getModel()->getActionHeap(), m_indexHeap);
    if (heap_act != this)
      DIE_IMPOSSIBLE;
  }
  xbt_heap_push(getModel()->getActionHeap(), this, min_finish);

  XBT_OUT();
}

void CpuTiAction::setPriority(double priority)
{
  XBT_IN("(%p,%g)", this, priority);
  m_priority = priority;
  p_cpu->modified(true);
  XBT_OUT();
}

double CpuTiAction::getRemains()
{
  XBT_IN("(%p)", this);
  p_cpu->updateRemainingAmount(surf_get_clock());
  XBT_OUT();
  return m_remains;
}

}
}

#endif /* SURF_MODEL_CPUTI_H_ */
