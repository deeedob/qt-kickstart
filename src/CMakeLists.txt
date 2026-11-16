# One folder and one QML module per feature, C++ and QML side by side. Dependencies point one way:
#   controls     the design system, knows no feature
#   canvas       drawing (Flux), uses libs/Shapes # demo
#   sketchbook   the list of sketches (MVVM), uses canvas # demo
#   app          the executable: composition root, logging, crash handling

add_subdirectory(Controls)
add_subdirectory(Notify)
add_subdirectory(Canvas) # demo
add_subdirectory(Sketchbook) # demo
add_subdirectory(App)
