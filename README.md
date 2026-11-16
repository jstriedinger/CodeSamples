# Jose Striedinger: Code Samples

Gameplay and AI programmer (Unity, Unreal, C++). These are excerpts from larger projects, so they don't build on their own. More at [jstriedinger.com](https://jstriedinger.com/).

## Unity (C#), *A Deep World*
- [FlockManager.cs](csharp/FlockManager.cs): flocking for hundreds of agents using the Job System, with pluggable behaviors and culling.
- [PredatorBase.cs](csharp/PredatorBase.cs) and [PredatorManager.cs](csharp/PredatorManager.cs): enemy state and module architecture, plus an event-driven manager that tracks the closest chaser. Behavior tree and pathfinding are third-party assets.

## Unreal (C++), *The Guardian*
- [DynamicCameraActor.cpp](cpp/DynamicCameraActor.cpp): camera that frames the player plus targets added or removed at runtime, with arm length computed from FOV and pitch.

## Custom engine (C++)
- [Flock.cpp](cpp/Flock.cpp): boids with obstacle avoidance and predator/prey roles, written from scratch. Details are under NDA.
