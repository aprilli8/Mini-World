# Mini-World

In this project, I created a modeling of a small world where objects in the scene can be observed and controlled through different first and third person camera perspectives. Written in C++ based on a codebase built on OpenGL.

Camera 1: OrbitCamera 
It always points at the world origin and offers a third person perspective of the scene.

Camera 2: FPSCamera 
It presents a first-person perspective of a human in the scene and is controllable by keyboard movements.

Camera 3: TrackingCamera 
It is a camera whose position is fixed in space that tracks and looks only at the plane camera.

Camera 4: ArbitraryCamera (plane camera)
It represents the first-person perspective of a plane flying in the air and is controllable by keyboard movements.
