#include "Field_Dimensions.hpp"


/*
(Length() 
Width() 
Border() 
LineWidth() 
GoalWidth() 
GoalDepth()
GoalHeight() 
PenaltyDist() 
PenaltyDiam() 
ArcRadius() 
CenterRadius() 
CenterDiameter() 
GoalFlat() 
FloorLength()
FloorWidth()
*/

const Field_Dimensions Field_Dimensions::Single_Field_Dimensions(
    6.050f, 4.050f, 0.250f, 0.010f, 0.700f, 0.180f, 0.160f, 0.750f, 0.010f,
    0.800f, 0.500f, 1.000f, 0.350f, 6.550f, 4.550f);

const Field_Dimensions Field_Dimensions::Double_Field_Dimensions(
    9.000f, 6.000f, 0.700f, 0.010f, 1.000f, 0.180f, 0.160f, 1.000f, 0.010f,
    1.000f, 0.500f, 1.000f, 0.500f, 10.400f, 7.400f);

const Field_Dimensions Field_Dimensions::RFCCambridge_SMALL(
    6.000f, 4.000f, 0.200f, 0.010f, 1.300f, 0.200f, 0.160f, 1.000f, 0.010f,
    0.500f, 0.500f, 1.000f, 0.500f, 8.000f, 5.400f);

const Field_Dimensions Field_Dimensions::Default_Dimensions =
    Field_Dimensions::RFCCambridge_SMALL;

Field_Dimensions Field_Dimensions::Current_Dimensions =
    Field_Dimensions::RFCCambridge_SMALL;


/*
World Space

These coordinates are absolute to the field and do not change vantage point 
egardless of your current team. They are based on a top view of the field 
with yellow on the left, and blue on the right. (0,0) is at field center 
and angle is counter-clockwise positive. 


Team Space

These coordinates are relative to each team and are based from the 
team's baseline and centered on the goal. Angle is counter-clockwise
 positive. 


*/