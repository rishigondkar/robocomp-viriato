/*
 *    Copyright (C)2020 by YOUR NAME HERE
 *
 *    This file is part of RoboComp
 *
 *    RoboComp is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    RoboComp is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with RoboComp.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef TRAJECTORYROBOT2D_H
#define TRAJECTORYROBOT2D_H

// Ice includes
#include <Ice/Ice.h>
#include <TrajectoryRobot2D.h>

#include <config.h>
#include "genericworker.h"

using namespace RoboCompTrajectoryRobot2D;

class TrajectoryRobot2DI : public virtual RoboCompTrajectoryRobot2D::TrajectoryRobot2D
{
public:
TrajectoryRobot2DI(GenericWorker *_worker);
	~TrajectoryRobot2DI();

	float changeTarget(const TargetPose  &target, const Ice::Current&);
	NavState getState(const Ice::Current&);
	float go(const TargetPose  &target, const Ice::Current&);
	float goBackwards(const TargetPose  &target, const Ice::Current&);
	float goReferenced(const TargetPose  &target, const float  xRef, const float  zRef, const float  threshold, const Ice::Current&);
	void mapBasedTarget(const NavigationParameterMap  &parameters, const Ice::Current&);
	void setHumanSpace(const PolyLineList  &polyList, const Ice::Current&);
	void stop(const Ice::Current&);

private:

	GenericWorker *worker;

};

#endif
