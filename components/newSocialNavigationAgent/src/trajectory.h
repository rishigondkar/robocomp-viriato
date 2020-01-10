//
// Created by robolab on 9/01/20.
//

#ifndef TRAJECTORY_H
#define TRAJECTORY_H


#include <innermodel/innermodel.h>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsPolygonItem>
#include <math.h>

#include "grid.h"
#include "CommonBehavior.h"
#include <AGMCommonBehavior.h>
#include <genericworker.h>
#include "innerviewer.h"

#define TILE_SIZE_ 250

// Map
struct TCell
{
    std::uint32_t id;
    bool free;
    bool visited;
    float cost;

    // method to save the value
    void save(std::ostream &os) const {	os << free << " " << visited; };
    void read(std::istream &is) {	is >> free >> visited ;};
};

class Trajectory {

public:

    std::vector<QString> robotNodes;
    std::vector<QString> restNodes;
    std::set<QString> excludedNodes;
    QList<QRectF> innerRegions;
    QRectF outerRegion;


    void initialize(const std::shared_ptr<InnerModel> &innerModel_,
            const std::shared_ptr<InnerViewer> &viewer_,
            const std::shared_ptr< RoboCompCommonBehavior::ParameterList > &configparams_,
            LaserPrx laser_prx,
            OmniRobotPrx omnirobot_proxy);
    void update(const std::shared_ptr<InnerModel> &innerModel_);
    void updatePolylines(const std::shared_ptr<InnerModel> &innerModel_, SNGPersonSeq persons_, SNGPolylineSeq intimate,SNGPolylineSeq personal,SNGPolylineSeq social,SNGPolylineSeq object,SNGPolylineSeq objectsblocking);

    bool checkRobotValidStateAtTargetFast(const QVec &targetPos, const QVec &targetRot) const;


private:
    // Grid
    using TDim = Grid<TCell>::Dimensions;
    TDim dimensions;

    Grid<TCell> grid;

    std::shared_ptr<InnerModel> innerModel;
    std::shared_ptr<InnerViewer> viewer;
    std::shared_ptr<RoboCompCommonBehavior::ParameterList> configparams;

    std::vector<QPolygonF> polylines_intimate,polylines_personal,polylines_social,polylines_objects_total,polylines_objects_blocked;
    std::vector<QPolygonF> prev_polylines_intimate = {},  prev_polylines_personal = {}, prev_polylines_social = {}, prev_polylines_objects_total = {}, prev_polylines_objects_blocked = {};

    std::vector<QPolygonF> toSetFree;
    std::vector<QPolygonF> toResetCost;

    void checkInitialCollisions();
    void recursiveIncludeMeshes(InnerModelNode *node, QString robotId, bool inside, std::vector<QString> &in, std::vector<QString> &out, std::set<QString> &excluded);

    void initGrid();
    void resetGrid();
    void updateFreeSpaceMap();



};

#endif
