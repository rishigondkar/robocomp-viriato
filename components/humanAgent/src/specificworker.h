/*
 *    Copyright (C)2019 by YOUR NAME HERE
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

/**
       \brief
       @author authorname
*/

// THIS IS AN AGENT


#ifndef SPECIFICWORKER_H
#define SPECIFICWORKER_H

#include <genericworker.h>
#include <innermodel/innermodel.h>
#ifdef USE_QTGUI
	#include <osgviewer/osgview.h>
	#include <innermodel/innermodelviewer.h>
	#include <map>

#endif

class SpecificWorker : public GenericWorker
{
Q_OBJECT

public:
    struct Pose3D
    {
        float x;
        float z;
        float ry;

    };

    std::map <int,Pose3D> humans_cam1; //rel id generico con sus posiciones
    std::map <int,Pose3D> humans_cam2; //rel id generico con sus posiciones

    int mesh = 1;
    int numcameras = 2;

    bool first = true;
    bool position_correct = false;
    bool rotation_correct = false;
    //----------------- intento de relacionar cara con esqueleto -----------------------//
	typedef map <int,int> relID;

	relID IDjointface; //mapa que relaciona los id procedentes del esqueleto con los de la cara
	relID IDfacegeneric;// rel cara con id generico de humanAgent
	relID IDjointgeneric;//rel id joint con id generico

	int IDgeneric = 0;
    bool backwards = false;

    //----------------------------------------------//

    SpecificWorker(MapPrx& mprx);
	~SpecificWorker();
	bool setParams(RoboCompCommonBehavior::ParameterList params);
	void includeInAGM(int id,Pose3D pose);
	void movePersonInAGM(int id, Pose3D pose);
    void getHumans();
    RoboCompHumanTracker::PersonList mixData(RoboCompHumanTracker::PersonList users1, RoboCompHumanTracker::PersonList users2);
//	bool removeFromAGM(int id);

    bool getPoseRot (jointListType list, Pose3D &personpose);
    int getIDgeneric (int idjoint, RoboCompFaceTracking::Faces faces);

	bool reloadConfigAgent();
	bool activateAgent(const ParameterMap &prs);
	bool setAgentParameters(const ParameterMap &prs);
	ParameterMap getAgentParameters();
	void killAgent();
	int uptimeAgent();
	bool deactivateAgent();
	StateStruct getAgentState();
	void structuralChange(const RoboCompAGMWorldModel::World &w);
	void edgesUpdated(const RoboCompAGMWorldModel::EdgeSequence &modifications);
	void edgeUpdated(const RoboCompAGMWorldModel::Edge &modification);
	void symbolUpdated(const RoboCompAGMWorldModel::Node &modification);
	void symbolsUpdated(const RoboCompAGMWorldModel::NodeSequence &modifications);



public slots:
	void compute();

private:

    QMutex *mux;
	InnerModel *innerModel;

#ifdef USE_QTGUI
	OsgView *osgView;
	InnerModelViewer *innerModelViewer;
#endif
	std::string action;
	ParameterMap params;
	AGMModel::SPtr worldModel;
	bool active;
	void regenerateInnerModelViewer();
	bool setParametersAndPossibleActivation(const ParameterMap &prs, bool &reactivated);
	bool sendModificationProposal(AGMModel::SPtr &worldModel, AGMModel::SPtr &newModel);

};

#endif
