
add_library(Challenges_objs OBJECT
    Encompass.cc
    FreeSki.cc
    GuitarFret.cc
    Lawn.cc
#    Polyomino.cc
#    RotatingCube.cc
    TowerDefence.cc
#    TripleThree.cc
)
set_target_properties(Challenges_objs PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

