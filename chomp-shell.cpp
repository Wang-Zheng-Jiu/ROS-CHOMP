/* Trials with CHOMP.
 *
 * Copyright (C) 2014 Roland Philippsen. All rights reserved.
 *
 * BSD license:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of
 *    contributors to this software may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR THE CONTRIBUTORS TO THIS SOFTWARE BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
   \file pp2d.cpp

   Interactive trials with CHOMP for point vehicles moving
   holonomously in the plane.  There is a fixed start and goal
   configuration, and you can drag a circular obstacle around to see
   how the CHOMP algorithm reacts to that.  Some of the computations
   involve guesswork, for instance how best to compute velocities, so
   a simple first-order scheme has been used.  This appears to produce
   some unwanted drift of waypoints from the start configuration to
   the end configuration.  Parameters could also be tuned a bit
   better.  Other than that, it works pretty nicely.
*/

#include "gfx.hpp"
#include <iostream>
#include <vector>
#include <Eigen/Dense>
#include <stdlib.h>
#include <sys/time.h>
#include <err.h>
#include "chomp.hpp"


typedef Eigen::VectorXd Vector;
typedef Eigen::MatrixXd Matrix;
typedef Eigen::Isometry3d Transform;

using namespace std;


//////////////////////////////////////////////////
// trajectory etc

static size_t const cdim (2);	// dimension of config space
static size_t const nq (20);	// number of q stacked into xi
static size_t const obs_dim (3);	// x,y,R
Vector xi;			// the trajectory (q_1, q_2, ...q_n)
Vector qs;			// the start config a.k.a. q_0
Vector qe;			// the end config a.k.a. q_(n+1)
Matrix obs;     //A matrix containning all obstavles, each column is (x,y,R) of the obstacle

void add_obs(double px,double py, double radius){
  //conservativeResize is used. It's a costy operation, but hopefully will not
  // be done too often.
  obs.conservativeResize(obs_dim,obs.cols()+1);
  obs.block(0,obs.cols()-1,obs_dim,1)<<px,py,radius;
}
enum { PAUSE, STEP, RUN } state;


static size_t grabbed (-1);
static Vector grab_offset (3);


//////////////////////////////////////////////////
// robot (one per waypoint)

class Robot
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  Robot ()
    : position_ (Vector::Zero(2))
  {
  }


  void update (Vector const & position)
  {
    if (position.size() != 2) {
      errx (EXIT_FAILURE, "Robot::update(): position has %zu DOF (but needs 2)",
	    (size_t) position.size());
    }
    position_ = position;

  }


  void draw () const
  {
    // translucent disk for base
    gfx::set_pen (1.0, 0.7, 0.7, 0.7, 0.5);
    gfx::fill_arc (position_[0], position_[1], radius_, 0.0, 2.0 * M_PI);

    // thick circle outline for base
    gfx::set_pen (3.0, 0.2, 0.2, 0.2, 1.0);
    gfx::draw_arc (position_[0], position_[1], radius_, 0.0, 2.0 * M_PI);

  }

  static double const radius_;

  Vector position_;

};

double const Robot::radius_ (0.5);

Robot rstart;
Robot rend;
vector <Robot> robots;


//////////////////////////////////////////////////

static void update_robots ()
{
  rstart.update (qs);
  rend.update (qe);
  if (nq != robots.size()) {
    robots.resize (nq);
  }
  for (size_t ii (0); ii < nq; ++ii) {
    robots[ii].update (xi.block (ii * cdim, 0, cdim, 1));
  }

}





static void cb_step ()
{
  state = STEP;
}


static void cb_run ()
{ /*
  if (RUN == state) {
    state = PAUSE;
  }
  else {
    state = RUN;
  } */
}


static void cb_jumble ()
{ /*
  for (size_t ii (0); ii < xidim; ++ii) {
    xi[ii] = double (rand()) / (0.1 * numeric_limits<int>::max()) - 5.0;
  }
  update_robots();
  */
}


static void cb_idle ()
{

  if (state == PAUSE) return;
  // end of "the" CHOMP iteration
  //////////////////////////////////////////////////

  chomp::run_chomp(qs,qe,xi,obs);
  update_robots ();

}


static void cb_draw ()
{
  //////////////////////////////////////////////////
  // set bounds

  Vector bmin (qs);
  Vector bmax (qs);
  for (size_t ii (0); ii < 2; ++ii) {
    if (qe[ii] < bmin[ii]) {
      bmin[ii] = qe[ii];
    }
    if (qe[ii] > bmax[ii]) {
      bmax[ii] = qe[ii];
    }
    for (size_t jj (0); jj < nq; ++jj) {
      if (xi[ii + cdim * jj] < bmin[ii]) {
	bmin[ii] = xi[ii + cdim * jj];
      }
      if (xi[ii + cdim * jj] > bmax[ii]) {
	bmax[ii] = xi[ii + cdim * jj];
      }
    }
  }

  gfx::set_view (bmin[0] - 2.0, bmin[1] - 2.0, bmax[0] + 2.0, bmax[1] + 2.0);

  //////////////////////////////////////////////////
  // robots

  rstart.draw();
  for (size_t ii (0); ii < robots.size(); ++ii) {
    robots[ii].draw();
  }
  rend.draw();

  //////////////////////////////////////////////////
  // trj

  gfx::set_pen (1.0, 0.2, 0.2, 0.2, 1.0);
  gfx::draw_line (qs[0], qs[1], xi[0], xi[1]);
  for (size_t ii (1); ii < nq; ++ii) {
    gfx::draw_line (xi[(ii-1) * cdim], xi[(ii-1) * cdim + 1], xi[ii * cdim], xi[ii * cdim + 1]);
  }
  gfx::draw_line (xi[(nq-1) * cdim], xi[(nq-1) * cdim + 1], qe[0], qe[1]);

  gfx::set_pen (5.0, 0.8, 0.2, 0.2, 1.0);
  gfx::draw_point (qs[0], qs[1]);
  gfx::set_pen (5.0, 0.5, 0.5, 0.5, 1.0);
  for (size_t ii (0); ii < nq; ++ii) {
    gfx::draw_point (xi[ii * cdim], xi[ii * cdim + 1]);
  }
  gfx::set_pen (5.0, 0.2, 0.8, 0.2, 1.0);
  gfx::draw_point (qe[0], qe[1]);

  //////////////////////////////////////////////////
  // handles
  for (size_t ii = 0; ii< obs.cols(); ++ii) {
    gfx::set_pen (1.0, 0.0, 0.0, 1.0, 0.2);
    gfx::fill_arc (obs(0,ii), obs(1,ii), obs(2,ii), 0.0, 2.0 * M_PI);
  }

}


static void cb_mouse (double px, double py, int flags)
{
if ((flags & gfx::MOUSE_RELEASE) && (flags & gfx::MOUSE_B3)) {
  //add new obstacle at that location
  add_obs (px, py, 2.0);
}

  else if (flags & gfx::MOUSE_PRESS) {
    for (size_t ii=0; ii<obs.cols() ; ++ii) {
      Vector offset (obs.block(0,ii,2,1));
      offset[0] -= px;
      offset[1] -= py;
      if (offset.norm() <= obs(2,ii)) {
      grab_offset = offset;
      grabbed = ii;
      state=RUN;
      break;
      }
    }
  }
  else if (flags & gfx::MOUSE_DRAG) {
    if (-1 != grabbed) {
      obs(0,grabbed) = px+grab_offset(0);
      obs(1,grabbed) = py+grab_offset(1);
    }
  }
  else if (flags & gfx::MOUSE_RELEASE) {
    grabbed = -1;
    state = PAUSE;
  }

}


int main()
{
  /*

  update_robots();


  gfx::add_button ("jumble", cb_jumble);
  gfx::add_button ("step", cb_step);
  gfx::add_button ("run", cb_run);
  gfx::main ("chomp", cb_idle, cb_draw, cb_mouse);
*/

  qs.resize (cdim);
  qs << -5.0, -5.0;
  qe.resize (cdim);
  qe << 7.0, 7.0;
  add_obs (3.0, 0.0, 2.0);
  add_obs (0.0, 3.0, 2.0);


  chomp::run_chomp(qs,qe,xi,obs);
  update_robots ();
  state = PAUSE;
  gfx::main ("chomp", cb_idle, cb_draw, cb_mouse);
}
