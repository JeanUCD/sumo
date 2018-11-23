/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNEDetectorExit.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Nov 2015
/// @version $Id$
///
//
/****************************************************************************/

// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>

#include <string>
#include <iostream>
#include <utility>
#include <netbuild/NBEdge.h>
#include <utils/geom/PositionVector.h>
#include <utils/common/RandHelper.h>
#include <utils/common/SUMOVehicleClass.h>
#include <utils/common/ToString.h>
#include <utils/geom/GeomHelper.h>
#include <utils/gui/windows/GUISUMOAbstractView.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/images/GUITextureSubSys.h>
#include <utils/gui/globjects/GUIGLObjectPopupMenu.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/images/GUITexturesHelper.h>
#include <utils/xml/SUMOSAXHandler.h>
#include <netedit/netelements/GNELane.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNENet.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <netedit/GNEViewParent.h>
#include <netedit/netelements/GNEEdge.h>

#include "GNEDetectorExit.h"
#include "GNEDetectorE3.h"
#include "GNEAdditionalHandler.h"


// ===========================================================================
// member method definitions
// ===========================================================================

GNEDetectorExit::GNEDetectorExit(GNEViewNet* viewNet, GNEAdditional* parent, GNELane* lane, double pos, bool friendlyPos, bool blockMovement) :
    GNEDetector(parent, viewNet, GLO_DET_EXIT, SUMO_TAG_DET_EXIT, pos, 0, "", "", friendlyPos, blockMovement),
    myLane(lane) {
}


GNEDetectorExit::~GNEDetectorExit() {}


bool 
GNEDetectorExit::isAdditionalValid() const {
    // with friendly position enabled position are "always fixed"
    if (myFriendlyPosition) {
        return true;
    } else {
        return (myPositionOverLane >= 0) && (myPositionOverLane <= myLane->getParentEdge().getNBEdge()->getFinalLength());
    }
}


std::string 
GNEDetectorExit::getAdditionalProblem() const {
    // declare variable for error position 
    std::string errorPosition;
    // check positions over lane
    if (myPositionOverLane < 0) {
        errorPosition = (toString(SUMO_ATTR_POSITION) + " < 0");
    }
    if (myPositionOverLane > myLane->getParentEdge().getNBEdge()->getFinalLength()) {
        errorPosition = (toString(SUMO_ATTR_POSITION) + " > lanes's length");
    }
    return errorPosition;
}


void 
GNEDetectorExit::fixAdditionalProblem() {
    // declare new position
    double newPositionOverLane = myPositionOverLane;
    // fix pos and lenght  checkAndFixDetectorPosition
    GNEAdditionalHandler::checkAndFixDetectorPosition(newPositionOverLane, myLane->getParentEdge().getNBEdge()->getFinalLength(), true);
    // set new position
    setAttribute(SUMO_ATTR_POSITION, toString(newPositionOverLane), myViewNet->getUndoList());
}


void
GNEDetectorExit::moveGeometry(const Position& offset) {
    // Calculate new position using old position
    Position newPosition = myMove.originalViewPosition;
    newPosition.add(offset);
    // filtern position using snap to active grid
    newPosition = myViewNet->snapToActiveGrid(newPosition);
    myPositionOverLane = myLane->getShape().nearest_offset_to_point2D(newPosition, false);
    // Update geometry
    updateGeometry(false);
}


void
GNEDetectorExit::commitGeometryMoving(GNEUndoList* undoList) {
    // commit new position allowing undo/redo
    undoList->p_begin("position of " + getTagStr());
    undoList->p_add(new GNEChange_Attribute(this, SUMO_ATTR_POSITION, toString(myPositionOverLane), true, myMove.firstOriginalLanePosition));
    undoList->p_end();
}


void
GNEDetectorExit::updateGeometry(bool updateGrid) {
    // first check if object has to be removed from grid (SUMOTree)
    if (updateGrid) {
        myViewNet->getNet()->removeGLObjectFromGrid(this);
    }

    // Clear all containers
    myGeometry.clearGeometry();

    // obtain position over lane
    double fixedPositionOverLane = myPositionOverLane > myLane->getParentEdge().getNBEdge()->getFinalLength() ? myLane->getParentEdge().getNBEdge()->getFinalLength() : myPositionOverLane < 0 ? 0 : myPositionOverLane;
    myGeometry.shape.push_back(myLane->getShape().positionAtOffset(fixedPositionOverLane * myLane->getLengthGeometryFactor()));

    // Save rotation (angle) of the vector constructed by points f and s
    myGeometry.shapeRotations.push_back(myLane->getShape().rotationDegreeAtOffset(fixedPositionOverLane) * -1);

    // Set block icon position
    myBlockIcon.position = myGeometry.shape.getLineCenter();

    // Set block icon rotation, and using their rotation for logo
    myBlockIcon.setRotation(myLane);

    // last step is to check if object has to be added into grid (SUMOTree) again
    if (updateGrid) {
        myViewNet->getNet()->addGLObjectIntoGrid(this);
    }

    // update E3 parent Geometry
    myFirstAdditionalParent->updateGeometry(updateGrid);
}


GNELane*
GNEDetectorExit::getLane() const {
    return myLane;
}


void
GNEDetectorExit::drawGL(const GUIVisualizationSettings& s) const {
    // Start drawing adding gl identificator
    glPushName(getGlID());

    // Push detector matrix
    glPushMatrix();
    glTranslated(0, 0, getType());

    // Set initial values
    if (isAttributeCarrierSelected()) {
        GLHelper::setColor(s.selectedAdditionalColor);
    } else {
        GLHelper::setColor(s.SUMO_color_E3Exit);
    }
    const double exaggeration = s.addSize.getExaggeration(s);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Push polygon matrix
    glPushMatrix();
    glScaled(exaggeration, exaggeration, 1);
    glTranslated(myGeometry.shape[0].x(), myGeometry.shape[0].y(), 0);
    glRotated(myGeometry.shapeRotations[0], 0, 0, 1);

    // draw details if isn't being drawn for selecting
    if (!s.drawForSelecting) {
        // Draw polygon
        glBegin(GL_LINES);
        glVertex2d(1.7, 0);
        glVertex2d(-1.7, 0);
        glEnd();
        glBegin(GL_QUADS);
        glVertex2d(-1.7, .5);
        glVertex2d(-1.7, -.5);
        glVertex2d(1.7, -.5);
        glVertex2d(1.7, .5);
        glEnd();

        // first Arrow
        glTranslated(1.5, 0, 0);
        GLHelper::drawBoxLine(Position(0, 4), 0, 2, .05);
        GLHelper::drawTriangleAtEnd(Position(0, 4), Position(0, 1), (double) 1, (double) .25);

        // second Arrow
        glTranslated(-3, 0, 0);
        GLHelper::drawBoxLine(Position(0, 4), 0, 2, .05);
        GLHelper::drawTriangleAtEnd(Position(0, 4), Position(0, 1), (double) 1, (double) .25);
    } else {
        // Draw square in drawy for selecting mode
        glBegin(GL_QUADS);
        glVertex2d(-1.7, 4.3);
        glVertex2d(-1.7, -.5);
        glVertex2d(1.7, -.5);
        glVertex2d(1.7, 4.3);
        glEnd();
    }

    // Pop polygon matrix
    glPopMatrix();

    // Pop detector matrix
    glPopMatrix();

    // Check if the distance is enought to draw details
    if (!s.drawForSelecting && ((s.scale * exaggeration) >= 10)) {
        // Push matrix
        glPushMatrix();
        // Traslate to center of detector
        glTranslated(myGeometry.shape.getLineCenter().x(), myGeometry.shape.getLineCenter().y(), getType() + 0.1);
        // Rotate depending of myBlockIcon.rotation
        glRotated(myBlockIcon.rotation, 0, 0, -1);
        //move to logo position
        glTranslated(1.9, 0, 0);
        // draw Entry logo if isn't being drawn for selecting
        if (s.drawForSelecting) {
            GLHelper::setColor(s.SUMO_color_E3Exit);
            GLHelper::drawBoxLine(Position(0, 1), 0, 2, 1);
        } else if (isAttributeCarrierSelected()) {
            GLHelper::drawText("E3", Position(), .1, 2.8, s.selectedAdditionalColor);
        } else {
            GLHelper::drawText("E3", Position(), .1, 2.8, s.SUMO_color_E3Exit);
        }
        //move to logo position
        glTranslated(1.7, 0, 0);
        // Rotate depending of myBlockIcon.rotation
        glRotated(90, 0, 0, 1);
        // draw Entry text if isn't being drawn for selecting
        if (s.drawForSelecting) {
            GLHelper::setColor(s.SUMO_color_E3Exit);
            GLHelper::drawBoxLine(Position(0, 1), 0, 2, 1);
        } else if (isAttributeCarrierSelected()) {
            GLHelper::drawText("Exit", Position(), .1, 1, s.selectedAdditionalColor);
        } else {
            GLHelper::drawText("Exit", Position(), .1, 1, s.SUMO_color_E3Exit);
        }
        // pop matrix
        glPopMatrix();
        // Show Lock icon depending of the Edit mode and if isn't being drawn for selecting
        if (!s.drawForSelecting) {
            myBlockIcon.draw(0.4);
        }
    }
    // Draw name if isn't being drawn for selecting
    if (!s.drawForSelecting) {
        drawName(getCenteringBoundary().getCenter(), s.scale, s.addName);
    }
    // check if dotted contour has to be drawn
    if (!s.drawForSelecting && (myViewNet->getACUnderCursor() == this)) {
        GLHelper::drawShapeDottedContour(getType(), myGeometry.shape[0], 3.4, 5, myGeometry.shapeRotations[0], 0, 2);
    }
    // pop gl identificator
    glPopName();
}


std::string
GNEDetectorExit::getAttribute(SumoXMLAttr key) const {
    switch (key) {
        case SUMO_ATTR_ID:
            return getAdditionalID();
        case SUMO_ATTR_LANE:
            return myLane->getID();
        case SUMO_ATTR_POSITION:
            return toString(myPositionOverLane);
        case SUMO_ATTR_FRIENDLY_POS:
            return toString(myFriendlyPosition);
        case GNE_ATTR_BLOCK_MOVEMENT:
            return toString(myBlockMovement);
        case GNE_ATTR_PARENT:
            return myFirstAdditionalParent->getID();
        case GNE_ATTR_SELECTED:
            return toString(isAttributeCarrierSelected());
        case GNE_ATTR_GENERIC:
            return getGenericParametersStr();
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


void
GNEDetectorExit::setAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    if (value == getAttribute(key)) {
        return; //avoid needless changes, later logic relies on the fact that attributes have changed
    }
    switch (key) {
        case SUMO_ATTR_ID:
        case SUMO_ATTR_LANE:
        case SUMO_ATTR_POSITION:
        case SUMO_ATTR_FRIENDLY_POS:
        case GNE_ATTR_BLOCK_MOVEMENT:
        case GNE_ATTR_PARENT:
        case GNE_ATTR_SELECTED:
        case GNE_ATTR_GENERIC:
            undoList->p_add(new GNEChange_Attribute(this, key, value));
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNEDetectorExit::isValid(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID:
            return isValidAdditionalID(value);
        case SUMO_ATTR_LANE:
            return (myViewNet->getNet()->retrieveLane(value, false) != nullptr);
        case SUMO_ATTR_POSITION:
            return canParse<double>(value);
        case SUMO_ATTR_FRIENDLY_POS:
            return canParse<bool>(value);
        case GNE_ATTR_BLOCK_MOVEMENT:
            return canParse<bool>(value);
        case GNE_ATTR_PARENT:
            return (myViewNet->getNet()->retrieveAdditional(SUMO_TAG_E3DETECTOR, value, false) != nullptr);
        case GNE_ATTR_SELECTED:
            return canParse<bool>(value);
        case GNE_ATTR_GENERIC:
            return isGenericParametersValid(value);
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}

void
GNEDetectorExit::setAttribute(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID:
            changeAdditionalID(value);
            break;
        case SUMO_ATTR_LANE:
            myLane = changeLane(myLane, value);
            break;
        case SUMO_ATTR_POSITION:
            myPositionOverLane = parse<double>(value);
            break;
        case SUMO_ATTR_FRIENDLY_POS:
            myFriendlyPosition = parse<bool>(value);
            break;
        case GNE_ATTR_BLOCK_MOVEMENT:
            myBlockMovement = parse<bool>(value);
            break;
        case GNE_ATTR_PARENT:
            changeFirstAdditionalParent(value);
            break;
        case GNE_ATTR_SELECTED:
            if (parse<bool>(value)) {
                selectAttributeCarrier();
            } else {
                unselectAttributeCarrier();
            }
            break;
        case GNE_ATTR_GENERIC:
            setGenericParametersStr(value);
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
    // Update Geometry after setting a new attribute (but avoided for certain attributes)
    if((key != SUMO_ATTR_ID) && (key != GNE_ATTR_GENERIC) && (key != GNE_ATTR_SELECTED)) {
        updateGeometry(true);
    }
}

/****************************************************************************/
