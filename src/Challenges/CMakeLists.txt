
add_library(Challenges_objs OBJECT
    Encompass.cc
    FreeSki.cc
    MazeExplorer.cc
    GuitarFret.cc
    Lawn.cc
    RotatingCube.cc
    TacticalStealth.cc
    TowerDefence.cc
    Werewolf.cc
#    Polyomino.cc
#    TripleThree.cc
)
set_target_properties(Challenges_objs PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
