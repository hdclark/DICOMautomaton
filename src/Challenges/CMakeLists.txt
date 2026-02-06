
add_library(Challenges_objs OBJECT
    Encompass.cc
    FreeSki.cc
    GuitarFret.cc
    Lawn.cc
    MoonRun.cc
    RotatingCube.cc
    TacticalStealth.cc
    TowerDefence.cc
#    Polyomino.cc
#    TripleThree.cc
)
set_target_properties(Challenges_objs PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
