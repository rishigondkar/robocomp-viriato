//
// Created by robolab on 22/07/20.
//


#include "actionExecution.h"

void ActionExecution::initialize(AGMModel::SPtr worldModel_)
{
    worldModel = worldModel_;
}

void ActionExecution::updateWordModel(AGMModel::SPtr worldModel_)
{
    worldModel = worldModel_;

}
void ActionExecution::update(std::string action_,  ParameterMap params_)
{
    qDebug()<< "ActionExecution" << __FUNCTION__;
    action = action_;
    params = params_;
    newActionReceived = true;
}



//////////////////////////////////missions//////////////////////////////////////////

ActionExecution::retActions ActionExecution::runActions(std::string action_,  ParameterMap params_)
{
    qDebug() << "---------------------------------------------------";
    qDebug() <<__FUNCTION__ <<"Checking ACTION: " << QString::fromStdString(action_);

    retActions ret;


    if (action_ == "changeroom")
    {
        ret = action_ChangeRoom(params_);
    }

    if (action_ == "gotoperson")
    {
        ret = action_GoToPerson(params_);
    }

    return ret;

}


ActionExecution::retActions ActionExecution::action_ChangeRoom(ParameterMap params_)
{

    qDebug()<<"-------------------------------"<< __FUNCTION__<< "-------------------------------";
    AGMModelSymbol::SPtr roomSymbol;
    AGMModelSymbol::SPtr robotSymbol;


    try
    {
        roomSymbol = worldModel->getSymbolByIdentifier(std::stoi(params_["room2"].value));
        robotSymbol = worldModel->getSymbolByIdentifier(std::stoi(params_["robot"].value));
    }
    catch( const Ice::Exception& ex)
    {
        std::cout << "Exception:: Error reading room and robot symbols" << ex << endl;
    }


    int32_t destRoomID = roomSymbol->identifier;
    std::string imName =roomSymbol->getAttribute("imName");
//    printf("imName: <%s>\n", imName.c_str());

    int32_t currentRoom = -1;

    try
    {

        for (AGMModelSymbol::iterator edge = robotSymbol->edgesBegin(worldModel);
             edge!=robotSymbol->edgesEnd(worldModel);
             edge++) {
            if (edge->getLabel()=="in") {
                const std::pair<int32_t, int32_t> symbolPair = edge->getSymbolPair();
                auto second = worldModel->getSymbol(symbolPair.second);
                if (second->typeString() == "room")
                {
                    currentRoom = second->identifier;
                }
            }
        }

    }

    catch( const Ice::Exception& ex)
    {
        std::cout << "Exception:: Cant get edge between robot and room:" << ex << endl;
    }

    if (currentRoom == -1)
    {
        qDebug()<< "shit";
        exit(-1);
    }

    bool needsReplanning = false;

    auto roomPolygon = getRoomPolyline(roomSymbol);
    QPointF newTarget = roomPolygon.boundingRect().center();


    if (currentRoom == destRoomID)
    {
        needsReplanning = false;
    }

    else if (newActionReceived)
    {

        newActionReceived = false;
        needsReplanning = true;

    }

    qDebug()<<  __FUNCTION__ << "Returning "<< needsReplanning << newTarget;
    return std::make_tuple(needsReplanning, newTarget);
};



ActionExecution::retActions ActionExecution::action_GoToPerson(ParameterMap params_)
{
    qDebug()<<"-------------------------------"<< __FUNCTION__<< "-------------------------------";
    AGMModelSymbol::SPtr personSymbol;
    AGMModelSymbol::SPtr robotSymbol;


    try
    {
        personSymbol = worldModel->getSymbolByIdentifier(std::stoi(params_["p"].value));
        robotSymbol = worldModel->getSymbolByIdentifier(std::stoi(params_["robot"].value));
    }
    catch( const Ice::Exception& ex)
    {
        std::cout << "Exception:: Error reading room and robot symbols" << ex << endl;
    }


    bool needsReplanning = false;
    QPointF newTarget = getPointInSocialSpace(personSymbol,robotSymbol);

    if (newActionReceived)
    {

        newActionReceived = false;
        needsReplanning = true;

    }

    qDebug()<<  __FUNCTION__ << "Returning "<< needsReplanning << newTarget;

    return std::make_tuple(needsReplanning, newTarget);


}

QPointF ActionExecution::getPointInSocialSpace(AGMModelSymbol::SPtr personSymbol,AGMModelSymbol::SPtr robotSymbol)
{
    localPerson person;

    auto id = personSymbol->identifier;
    AGMModelSymbol::SPtr personParent = worldModel->getParentByLink(id, "RT");
    AGMModelEdge& edgeRT = worldModel->getEdgeByIdentifiers(personParent->identifier, id, "RT");

    person.id = id;
    person.x = str2float(edgeRT.attributes["tx"]);
    person.z = str2float(edgeRT.attributes["tz"]);

    localPerson robot;

    auto id_r = robotSymbol->identifier;
    AGMModelSymbol::SPtr robotParent = worldModel->getParentByLink(id_r, "RT");
    AGMModelEdge& edgeRT_r = worldModel->getEdgeByIdentifiers(robotParent->identifier, id_r, "RT");

    robot.id = id_r;
    robot.x = str2float(edgeRT_r.attributes["tx"]);
    robot.z = str2float(edgeRT_r.attributes["tz"]);


    AGMModelSymbol::SPtr personalSpace;

    for (AGMModelSymbol::iterator edge = personSymbol->edgesBegin(worldModel);
         edge!=personSymbol->edgesEnd(worldModel);
         edge++) {
        if (edge->getLabel()=="has") {
            const std::pair<int32_t, int32_t> symbolPair = edge->getSymbolPair();
            personalSpace = worldModel->getSymbolByIdentifier(symbolPair.second);
            break;
        }
    }


    QString personal = QString::fromStdString(personalSpace->getAttribute("personal"));

    vector<QPolygonF> polygonSeq;

    QPolygonF personalPolygon;

    for(auto pol: personal.split(";;"))
    {
        if(pol.size() == 0)
            continue;

        personalPolygon = QPolygonF();

        for (auto pxz : pol.split(";"))
        {
            auto p = pxz.split(" ");

            if (p.size() != 2)
                continue;

            auto x = std::stof(p[0].toStdString());
            auto z = std::stof(p[1].toStdString());

            personalPolygon << QPointF(x,z);
        }

        //Puede haber varios espacios sociales, se comprueba que la persona esta contenida en el espacio social
        if (personalPolygon.containsPoint(QPointF(person.x,person.z),Qt::OddEvenFill))
            break;
    }

    QLineF line(QPointF(person.x,person.z),QPointF(robot.x,robot.z));

    for (int i = 0; i < 10; i++)
    {
        float step = i/10.0f;

        QPointF point = line.pointAt(step);

        //El punto más cercano a la persona dentro del espacio social será el primer punto no contenido en el personal
        if (!personalPolygon.containsPoint(point,Qt::OddEvenFill)){
            return point;
        }
    }



}

QPolygonF ActionExecution::getRoomPolyline(AGMModelSymbol::SPtr roomSymbol)
{

    QPolygonF polygon;

    try {
        auto polyline = QString::fromStdString(roomSymbol->getAttribute("polyline"));

        for (auto pxz : polyline.split(";"))
        {
            auto p = pxz.split(" ");

            if (p.size() != 2)
                continue;

            auto x = std::stof(p[0].toStdString());
            auto z = std::stof(p[1].toStdString());

            polygon << QPointF(x,z);
        }

        return polygon;

    }

    catch (std::exception& e) {
        std::cout << "Exception reading room "<< roomSymbol->identifier << " polyline: " << e.what() << std::endl;
    }

}