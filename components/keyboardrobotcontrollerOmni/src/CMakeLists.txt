cmake_minimum_required(VERSION 2.6)
PROJECT(  keyboardrobotcontrollerOmni )

INCLUDE( $ENV{ROBOCOMP}/cmake/robocomp.cmake )

ROBOCOMP_IDSL_TO_ICE( CommonBehavior GenericBase OmniRobot )
