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
#include "specificworker.h"

/**
* \brief Default constructor
*/
SpecificWorker::SpecificWorker(MapPrx& mprx) : GenericWorker(mprx)
{

	active = false;
	worldModel = AGMModel::SPtr(new AGMModel());
	worldModel->name = "worldModel";

    innerModel = std::make_shared<InnerModel>();

}

/**
* \brief Default destructor
*/
SpecificWorker::~SpecificWorker()
{
	std::cout << "Destroying SpecificWorker" << std::endl;
	emit t_compute_to_finalize();
}

bool SpecificWorker::setParams(RoboCompCommonBehavior::ParameterList params)
{
    confParams  = std::make_shared<RoboCompCommonBehavior::ParameterList>(params);

	defaultMachine.start();

	return true;
}

void SpecificWorker::initialize(int period)
{
    QMutexLocker lockIM(mutex);

    std::cout << "Initialize worker" << std::endl;

    connect(autoMov_checkbox, SIGNAL(clicked()),this, SLOT(checkRobotAutoMovState()));
    connect(robotMov_checkbox, SIGNAL(clicked()),this, SLOT(moveRobot()));
    connect(permission_checkbox, SIGNAL(clicked()),this, SLOT(checkRobotPermission()));

    connect(ki_slider, SIGNAL (valueChanged(int)),this,SLOT(forcesSliderChanged(int)));
    connect(ke_slider, SIGNAL (valueChanged(int)),this,SLOT(forcesSliderChanged(int)));

    connect(send_button, SIGNAL(clicked()),this, SLOT(sendRobotTo()));


    forcesSliderChanged();
    moveRobot();


#ifdef USE_QTGUI
	viewer = std::make_shared<InnerViewer>(innerModel, "Social Navigation");  //InnerViewer copies internally innerModel so it has to be resynchronized
#endif

    try
    {
        RoboCompAGMWorldModel::World w = agmexecutive_proxy->getModel();
        AGMExecutiveTopic_structuralChange(w);
        robotID = worldModel->getIdentifierByType("robot");


    }
    catch(...)
    {
        printf("The executive is probably not running, waiting for first AGM model publication...");
    }

    navigation.initialize(innerModel, viewer, confParams, omnirobot_proxy);
    actionExecution.initialize(worldModel);



    qDebug()<<"Classes initialized correctly";

    getPeopleBlocking();

    this->Period = period;
    timer.start(period);


    emit this->t_initialize_to_compute();

}



void SpecificWorker::compute()
{
//    qDebug()<< __FUNCTION__;

//    static QTime reloj = QTime::currentTime();

    bool needsReplaning = false;

    if(personalSpacesChanged)
	{
    	getPersonsFromModel();
        auto [intimate,personal,social] = getPolylinesFromModel();
    	navigation.updatePersonalPolylines(intimate,personal,social);
		personalSpacesChanged = false;
		needsReplaning = true;
	}

	if(affordancesChanged)
	{
        auto [mapCostObjects, totalAffordances, blockedAffordances] = getAffordancesFromModel();

        navigation.updateAffordancesPolylines(mapCostObjects,totalAffordances,blockedAffordances);
		affordancesChanged = false;
		needsReplaning = true;
	}

    QMutexLocker lockIM(mutex);

    RoboCompLaser::TLaserData laserData = updateLaser();

	navigation.update(totalPersons, laserData, needsReplaning);

//    static QTime reloj = QTime::currentTime();

    viewer->run();
//    qDebug()<< "viewer " << reloj.restart();


	if (!totalPersons.empty())
        checkHumanBlock();


    if (active)
    {
        auto [newTarget, target] = actionExecution.runActions(action,params);
        if (newTarget)
        {
            qDebug()<< "new target "<< target;
            bool reachable = navigation.newTarget(target);

            if (!reachable)
                qDebug()<< "Can't reach new target ";

        }

        if(robotBlocked)
            checkRobotBlock();

    }

}

//Check if the robot is still blocked
void SpecificWorker::checkRobotBlock()
{
    qDebug()<<__FUNCTION__;
    auto [_, target] = actionExecution.runActions(actionBlocked,paramsBlocked);
    if(navigation.isPointVisitable(target))
    {
        qDebug()<< "---- ROBOT NOT BLOCKED ----";
        robotBlocked = false;
        actionBlocked = "";
        planBlocked = "";
        paramsBlocked.clear();
    }

}

void SpecificWorker::checkHumanBlock()
{
	QMutexLocker lockIM(mutex);

	newModel = AGMModel::SPtr(new AGMModel(worldModel));

	bool edgesChanged = false;


	auto blockingIDs = navigation.blockIDs;
    auto softBlockingIDs = navigation.softBlockIDs;
    auto affBlockingIDs = navigation.affBlockIDs;


    if((prev_blockingIDs != blockingIDs) or (prev_softBlockingIDs != softBlockingIDs) or (prev_affBlockingIDs != affBlockingIDs))
    {
        qDebug()<< "blocking - prev: " << prev_blockingIDs << " current: " << blockingIDs;
		qDebug()<< "aff blocking - prev: " << prev_affBlockingIDs << " current: " << affBlockingIDs;
		qDebug()<< "SOFT blocking - prev: " << prev_softBlockingIDs << " current: " << softBlockingIDs;

           ////////////////////// block /////////////////////////////////
          //  Si solo hay una persona bloqueando el camino --block   ///
         // Si son dos -- estan interaccionando -- strongInterBlock ///
        //////////////////////////////////////////////////////////////
        string edgeName;

		vector<string> edgeNames{"is_blocking" , "strongInterBlock"};

		for (auto edgeName : edgeNames)
		{
			for(auto id: prev_blockingIDs)
                removeEdgeModel(id,robotID,edgeName);
		}

        if(blockingIDs.size() == 1) edgeName = "is_blocking";
        else edgeName = "strongInterBlock";

        for(auto id: blockingIDs)
           addEdgeModel(id,robotID,edgeName);



        ////////////////////////// affordanceBlock ////////////////////////

        string newEdgeName = "affordanceBlock";

        for(auto id: prev_affBlockingIDs)
            removeEdgeModel(id,robotID,newEdgeName);


        for(auto id: affBlockingIDs)
            addEdgeModel(id,robotID,newEdgeName);



        ////////////////////////// softBlock //////////////////////////////

        for(auto groupID: prev_softBlockingIDs)
        {
			vector<string> edgeNames{"softBlock" , "softInterBlock"};

			for (auto edgeName : edgeNames)
			{
				for(auto id : groupID)
                    removeEdgeModel(id,robotID,edgeName);
			}
        }

        for(auto groupID: softBlockingIDs)
        {
			if(groupID.size() == 1) edgeName = "softBlock";
			else edgeName = "softInterBlock";

        	for(auto id : groupID)
                addEdgeModel(id,robotID,edgeName);

        }


        edgesChanged = true;
    }

    else
		edgesChanged = false;


    if  (active and
            (blockingIDs.size() != 0 or affBlockingIDs.size() != 0) and
                ((prev_blockingIDs != blockingIDs) or  (prev_affBlockingIDs != affBlockingIDs)))
    {
        if(addEdgeModel(robotID,robotID,"blocked"))
            edgesChanged = true;


        robotBlocked = true;

        if (planBlocked == "")
        {
            planBlocked = currentPlan;
            paramsBlocked = params;
            actionBlocked = action;
        }
    }

    if (!robotBlocked)
    {
        if(removeEdgeModel(robotID,robotID,"blocked"))
            edgesChanged = true;


        actionBlocked = "";
        planBlocked = "";
        paramsBlocked.clear();

    }



	if(edgesChanged){
		try
		{
            qDebug()<< "sendModificationProposal";
            sendModificationProposal(worldModel, newModel);
		}
		catch(...)
		{
			std::cout<<"No se puede actualizar worldModel"<<std::endl;
		}
	}



    prev_blockingIDs = blockingIDs;
    prev_affBlockingIDs = affBlockingIDs;
    prev_softBlockingIDs = softBlockingIDs;

}

bool SpecificWorker::removeEdgeModel(int32_t id1, int32_t id2, string edgeName)
{
//    qDebug()<< __FUNCTION__ << id1 << id2 <<  QString::fromStdString(edgeName);

    try
    {
        newModel->removeEdgeByIdentifiers(id1, id2, edgeName);
        qDebug ()<<" Se elimina el enlace " << QString::fromStdString(edgeName) << " de " << id1;
        return true;
    }

    catch(...)
    {
//        std::cout<<__FUNCTION__<<" No existe el enlace " << edgeName  <<std::endl;
        return false;
    }

}

bool SpecificWorker::addEdgeModel(int32_t id1, int32_t id2, string edgeName)
{
    try
    {
        newModel->addEdgeByIdentifiers(id1, id2, edgeName);
        qDebug ()<<" Se añade el enlace " << QString::fromStdString(edgeName) << " de " << id1;
    }

    catch(...)
    {
        std::cout<<__FUNCTION__<<" No existe el enlace " << edgeName <<std::endl;

    }
}


RoboCompLaser::TLaserData  SpecificWorker::updateLaser()
{
//	qDebug()<<__FUNCTION__;

	RoboCompLaser::TLaserData laserData;

    try
    {
		laserData  = laser_proxy->getLaserData();
    }

    catch(const Ice::Exception &e){ std::cout <<"Can't connect to laser --" <<e.what() << std::endl; };

    return laserData;
}


void SpecificWorker::getPersonsFromModel()
{
	totalPersons.clear();
	auto vectorPersons = worldModel->getSymbolsByType("person");

	for (auto p: vectorPersons) {
		localPerson person;

		auto id = p->identifier;
		AGMModelSymbol::SPtr personParent = worldModel->getParentByLink(id, "RT");
		AGMModelEdge& edgeRT = worldModel->getEdgeByIdentifiers(personParent->identifier, id, "RT");

		person.id = id;
		person.x = str2float(edgeRT.attributes["tx"]);
		person.z = str2float(edgeRT.attributes["tz"]);
		person.angle = str2float(edgeRT.attributes["ry"]);

		totalPersons.push_back(person);
	}
}

void SpecificWorker::getPeopleBlocking()
{
    auto vectorPersons = worldModel->getSymbolsByType("person");
    for (auto person : vectorPersons)
    {

        for (AGMModelSymbol::iterator edge = person->edgesBegin(worldModel);
             edge!=person->edgesEnd(worldModel);
             edge++) {
            if (edge->getLabel()=="is_blocking") {
                const std::pair<int32_t, int32_t> symbolPair = edge->getSymbolPair();
                prev_blockingIDs.push_back(symbolPair.first);
            }
        }
    }

}


SpecificWorker::retPersonalSpaces SpecificWorker::getPolylinesFromModel()
{
	qDebug()<<__FUNCTION__;

    vector<QPolygonF> intimatePolygon;
    vector<QPolygonF> personalPolygon;
    vector<QPolygonF> socialPolygon;

    vector <vector<QPolygonF>> polylinesSeq {intimatePolygon, personalPolygon, socialPolygon};

    auto personalSpaces = worldModel->getSymbolsByType("personalSpace");
    vector<int> IDsAlreadyIncluded;

    for( auto space : personalSpaces)
    {
        int owner = -1;

        for (AGMModelSymbol::iterator edge = space->edgesBegin(worldModel);
             edge!=space->edgesEnd(worldModel);
             edge++) {
            if (edge->getLabel()=="has") {
                const std::pair<int32_t, int32_t> symbolPair = edge->getSymbolPair();
                owner = symbolPair.first;
            }
        }

        QString intimate = QString::fromStdString(space->getAttribute("intimate"));
        QString personal = QString::fromStdString(space->getAttribute("personal"));
        QString social = QString::fromStdString(space->getAttribute("social"));

        QString sharedWith = QString::fromStdString(space->getAttribute("sharedWith"));
        vector<int> sharedWithIDs;

        for (auto sh : sharedWith.split(" ")) {
            if (sh.toInt() != 0)
                sharedWithIDs.push_back(sh.toInt());
        }

        if(!sharedWithIDs.empty())
        {
            qDebug()<< "Searching for " << owner;
            if( std::find(std::begin(IDsAlreadyIncluded), std::end(IDsAlreadyIncluded), owner) != std::end(IDsAlreadyIncluded))
                continue;
        }


        vector<QString> polylinesStr = {intimate,personal,social};

        for (auto &&[str, polygonSeq] : iter::zip(polylinesStr, polylinesSeq))
        {
            for(auto pol: str.split(";;"))
            {
                if(pol.size() == 0)
                    continue;

                QPolygonF polygon;

                for (auto pxz : pol.split(";"))
                {
                    auto p = pxz.split(" ");

                    if (p.size() != 2)
                        continue;

                    auto x = std::stof(p[0].toStdString());
                    auto z = std::stof(p[1].toStdString());

                    polygon << QPointF(x,z);
                }

                polygonSeq.push_back(polygon);
            }
		}


		for(auto id : sharedWithIDs) {
            if (std::find(std::begin(IDsAlreadyIncluded), std::end(IDsAlreadyIncluded), id)==std::end(IDsAlreadyIncluded)) {
                IDsAlreadyIncluded.push_back(id);
            }
        }


    }
    return std::make_tuple(polylinesSeq[0],polylinesSeq[1],polylinesSeq[2]);

}

SpecificWorker::retAffordanceSpaces SpecificWorker::getAffordancesFromModel()
{

    qDebug()<<__FUNCTION__;

    vector<QPolygonF> totalAffordances;
    vector<QPolygonF> blockedAffordances;
    std::map<float,vector<QPolygonF>> mapCostObjects;

    auto affordanceSpaces = worldModel->getSymbolsByType("affordanceSpace");


    for( auto affordance : affordanceSpaces)
    {
        QPolygonF object;

        QString polyline = QString::fromStdString(affordance->getAttribute("affordance"));
        float cost = std::stof(affordance->getAttribute("cost"));
        bool interacting = (affordance->getAttribute("interacting") == "1");

        for(auto pol: polyline.split(";;"))
        {
            if(pol.size() == 0)
                continue;

            for (auto pxz : pol.split(";"))
            {
                auto p = pxz.split(" ");

                if (p.size() != 2)
                    continue;

                auto x = std::stof(p[0].toStdString());
                auto z = std::stof(p[1].toStdString());

                object<< QPointF(x,z);
            }

            mapCostObjects[cost].push_back(object);
            totalAffordances.push_back(object);
            if(interacting) blockedAffordances.push_back(object);

        }
    }


    qDebug()<< "END "<< __FUNCTION__;

    return std::make_tuple(mapCostObjects,totalAffordances,blockedAffordances);
}


void  SpecificWorker::moveRobot()
{
    qDebug()<<__FUNCTION__;

    if(robotMov_checkbox->checkState() == Qt::CheckState(2))
    {
        autoMov_checkbox->setEnabled(true);
        navigation.moveRobot = true;
		navigation.stopMovingRobot = false;
    }

    else
    {
        if(navigation.current_target.active.load())
			navigation.stopMovingRobot = true;

        else
		{
            navigation.moveRobot = false;
			navigation.stopMovingRobot = false;
		}

        autoMov_checkbox->setEnabled(false);

    }

}


void  SpecificWorker::checkRobotAutoMovState()
{
	qDebug()<<__FUNCTION__;

	if(autoMov_checkbox->checkState() == Qt::CheckState(2))
	{
		navigation.robotAutoMov = true;
		navigation.newRandomTarget();
	}

	else
    {
        navigation.robotAutoMov = false;
    }

}


void SpecificWorker::sendRobotTo()
{
    auto x =  x_spinbox->value();
    auto z =  z_spinbox->value();

    navigation.newTarget(QPointF(x,z));

}

void SpecificWorker::checkRobotPermission()
{
    qDebug()<< __FUNCTION__;
    if(permission_checkbox->checkState() == Qt::CheckState(2)) {
        robotBlocked = false;
    }
}

void SpecificWorker::
forcesSliderChanged(int value)
{

    navigation.KI = (float) ki_slider -> sliderPosition();
    navigation.KE = (float) ke_slider -> sliderPosition();

}


void SpecificWorker::sm_compute()
{
//	std::cout<<"Entered state compute"<<std::endl;
	compute();
}

void SpecificWorker::sm_initialize()
{
	std::cout<<"Entered initial state initialize"<<std::endl;
}

void SpecificWorker::sm_finalize()
{
	std::cout<<"Entered final state finalize"<<std::endl;
}



////////////////////////// SUBSCRIPTIONS /////////////////////////////////////////////

void SpecificWorker::RCISMousePicker_setPick(const Pick &myPick)
{
    navigation.newTarget(QPointF(myPick.x,myPick.z));
}

void SpecificWorker::SocialRules_objectsChanged(const SRObjectSeq &objectsAffordances)
{
    //subscribesToCODE
    qDebug() << __FUNCTION__ << objectsAffordances.size();

    objects_seq = objectsAffordances;

	affordancesChanged = true;

}

void SpecificWorker::SocialRules_personalSpacesChanged(const RoboCompSocialNavigationGaussian::SNGPolylineSeq &intimateSpaces, const RoboCompSocialNavigationGaussian::SNGPolylineSeq &personalSpaces, const RoboCompSocialNavigationGaussian::SNGPolylineSeq &socialSpaces)
{
    qDebug() << __FUNCTION__;

	intimate_seq = intimateSpaces;
	personal_seq = personalSpaces;
	social_seq = socialSpaces;

	personalSpacesChanged = true;
}

///////////////////////////////////////////////////////////////////////////////////////

bool SpecificWorker::AGMCommonBehavior_activateAgent(const ParameterMap &prs)
{
    qDebug()<<__FUNCTION__;

	bool activated = false;
	if (setParametersAndPossibleActivation(prs, activated))
	{
		if (not activated)
		{
			return activate(p);
		}
	}
	else
	{
		return false;
	}
	return true;
}

bool SpecificWorker::AGMCommonBehavior_deactivateAgent()
{
//implementCODE
	return deactivate();
}

ParameterMap SpecificWorker::AGMCommonBehavior_getAgentParameters()
{
//implementCODE
	return params;
}

StateStruct SpecificWorker::AGMCommonBehavior_getAgentState()
{
//implementCODE
	StateStruct s;
	if (isActive())
	{
		s.state = RoboCompAGMCommonBehavior::StateEnum::Running;
	}
	else
	{
		s.state = RoboCompAGMCommonBehavior::StateEnum::Stopped;
	}
	s.info = p.action.name;
	return s;
}

void SpecificWorker::AGMCommonBehavior_killAgent()
{
//implementCODE

}

bool SpecificWorker::AGMCommonBehavior_reloadConfigAgent()
{
//implementCODE
	return true;
}

bool SpecificWorker::AGMCommonBehavior_setAgentParameters(const ParameterMap &prs)
{
//implementCODE
	bool activated = false;
	return setParametersAndPossibleActivation(prs, activated);
}

int SpecificWorker::AGMCommonBehavior_uptimeAgent()
{
//implementCODE
	return 0;
}

void SpecificWorker::AGMExecutiveTopic_selfEdgeAdded(const int nodeid, const string &edgeType, const RoboCompAGMWorldModel::StringDictionary &attributes)
{
    qDebug()<<__FUNCTION__;
	QMutexLocker lockIM(mutex);
	try { worldModel->addEdgeByIdentifiers(nodeid, nodeid, edgeType, attributes); } catch(...){ printf("Couldn't add an edge. Duplicate?\n"); }

	try {
		innerModel.reset(AGMInner::extractInnerModel(worldModel));

        viewer->reloadInnerModel(innerModel);
		navigation.updateInnerModel(innerModel);


	} catch(...) { printf("Can't extract an InnerModel from the current model.\n"); }
}

void SpecificWorker::AGMExecutiveTopic_selfEdgeDeleted(const int nodeid, const string &edgeType)
{
    qDebug()<<__FUNCTION__;

    QMutexLocker lockIM(mutex);
	try { worldModel->removeEdgeByIdentifiers(nodeid, nodeid, edgeType); } catch(...) { printf("Couldn't remove an edge\n"); }

	try {
        innerModel.reset(AGMInner::extractInnerModel(worldModel));

        viewer->reloadInnerModel(innerModel);
		navigation.updateInnerModel(innerModel);


    } catch(...) { printf("Can't extract an InnerModel from the current model.\n"); }
}


void SpecificWorker::AGMExecutiveTopic_edgeUpdated(const RoboCompAGMWorldModel::Edge &modification)
{

    QMutexLocker lockIM(mutex);


    AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);
	AGMInner::updateImNodeFromEdge(worldModel, modification, innerModel.get());



}

void SpecificWorker::AGMExecutiveTopic_edgesUpdated(const RoboCompAGMWorldModel::EdgeSequence &modifications)
{
//subscribesToCODE

//	qDebug() << __FUNCTION__;

//    if(!specificWorkerInitialized)
//    {
//        QMutexLocker lockIM(mutex);
//    }

    QMutexLocker lockIM(mutex);

	for (auto modification : modifications)
	{
		AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);
		AGMInner::updateImNodeFromEdge(worldModel, modification, innerModel.get());
	}


}

void SpecificWorker::AGMExecutiveTopic_structuralChange(const RoboCompAGMWorldModel::World &w)
{
	qDebug() << __FUNCTION__;


    QMutexLocker lockIM(mutex);


	try {
		AGMModelConverter::fromIceToInternal(w, worldModel);
		innerModel.reset(AGMInner::extractInnerModel(worldModel));

		viewer->reloadInnerModel(innerModel);
		navigation.updateInnerModel(innerModel);
		actionExecution.updateWordModel(worldModel);

	} catch(...) { qDebug()<<__FUNCTION__<<"Can't extract an InnerModel from the current model."; }

	personalSpacesChanged = true;
	affordancesChanged = true;


}

void SpecificWorker::AGMExecutiveTopic_symbolUpdated(const RoboCompAGMWorldModel::Node &modification)
{
   // qDebug()<< __FUNCTION__;

	QMutexLocker locker(mutex);
	AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);

    if (modification.nodeType == "personalSpace")
        personalSpacesChanged = true;

    if (modification.nodeType == "affordanceSpace")
        affordancesChanged = true;
}

void SpecificWorker::AGMExecutiveTopic_symbolsUpdated(const RoboCompAGMWorldModel::NodeSequence &modifications)
{

   // qDebug()<< __FUNCTION__;

	QMutexLocker l(mutex);

    for (auto modification : modifications)
    {
        AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);

        for(auto node : modifications)
        {
            if (node.nodeType == "personalSpace")
                personalSpacesChanged = true;

            if (modification.nodeType == "affordanceSpace")
                affordancesChanged = true;
        }

    }

}

bool SpecificWorker::setParametersAndPossibleActivation(const ParameterMap &prs, bool &reactivated)
{

	printf("<<< ------------------------- setParametersAndPossibleActivation -------------------------\n");
	// We didn't reactivate the component
	reactivated = false;

	// Update parameters
	params.clear();
	for (ParameterMap::const_iterator it=prs.begin(); it!=prs.end(); it++)
	{
		params[it->first] = it->second;
		qDebug()<< QString::fromStdString(it->first) << " " << QString::fromStdString(it->second.value);
	}

	vector<QString> plans;
	bool blockedPlanInPlans = false;

    QString planList = QString::fromStdString(params["plan"].value);
    qDebug()<< "Blocked plan "<< planBlocked;

	for (auto plan : planList.split("\n"))
    {
        if(plan == "")
            continue;

        if(planBlocked == plan)
            blockedPlanInPlans = true;

	    plans.push_back(plan);

    }

    robotBlocked = blockedPlanInPlans;

    if (plans.size()!= 0)
        currentPlan = plans[0];

	try
	{
		action = params["action"].value;
		std::transform(action.begin(), action.end(), action.begin(), ::tolower);
		//TYPE YOUR ACTION NAME
		if (action != "none")
		{
			active = true;
		}
		else
		{
		    qDebug()<< " End mission .....";
            active = false;

            if (navigation.current_target.blocked.load() )
		    {
                navigation.stopRobot();
                navigation.deactivateTarget();
            }

            robotBlocked = false;
            actionBlocked = action;
            currentPlan = "none";
            planBlocked = "";
            paramsBlocked.clear();
        }
	}
	catch (...)
	{
		printf("exception in setParametersAndPossibleActivation %d\n", __LINE__);
		return false;
	}

	// Check if we should reactivate the component
	if (active)
	{
		active = true;
		reactivated = true;

        actionExecution.update(action,params);

    }




    printf("<<< ------------------------- setParametersAndPossibleActivation -------------------------\n");

	return true;
}

void SpecificWorker::sendModificationProposal(AGMModel::SPtr &worldModel, AGMModel::SPtr &newModel)
{
    qDebug()<<__FUNCTION__;
    QMutexLocker locker(mutex);

    try
    {
        AGMMisc::publishModification(newModel, agmexecutive_proxy, std::string( "SocialnavigationAgent"));
    }
    catch(const RoboCompAGMExecutive::Locked &e)
    {
    }
    catch(const RoboCompAGMExecutive::OldModel &e)
    {
        printf("modelo viejo\n");
    }
    catch(const RoboCompAGMExecutive::InvalidChange &e)
    {
        printf("modelo invalido\n");
    }
    catch(const Ice::Exception& e)
    {
        exit(1);
    }
}
