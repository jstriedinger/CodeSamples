using System;
using System.Collections.Generic;
using System.Linq;
using Unity.Burst;
using Unity.Collections;
using UnityEngine;
using Random = UnityEngine.Random;
using Unity.Jobs;
using Sirenix.OdinInspector;
using UnityEngine.Jobs;

using Unity.Mathematics;
public struct BoidData
{
    public float2 Position;
    public float2 Forward;
}


[BurstCompile]
public struct FlockDirectionJob : IJobParallelFor
{
    //Job that takes care of all the flocking behavior calculations
    
    [Unity.Collections.ReadOnly] public NativeArray<BoidData> Agents;
    public NativeArray<float2> Output;
    public NativeArray<float> SpeedMultiplierOutput;

    public FlockGoalType GoalType;
    public FlockContainmentType ContainmentType;
    public FlockFollowAfterType FollowAfterType;

    // ------------ Core behavior ---------------- //
    //alignment, cohesion and separation variables
    public float NeighborDistSqr, MaxNeighbors;
    public float AlignmentWeight, CohesionWeight, SeparationWeight;
    
    // ----------- Avoid Behavior --------------- //
    public bool AvoidActive;
    public bool CanHardAvoid;
    //This is like sayng I am intentionally ignoring the per-thread restriction. I know what I’m doing
    [NativeDisableParallelForRestriction]
    public NativeArray<float2> HardAvoidPositions;
    public float HardAvoidanceDistanceSqr;
    public float HardAvoidanceWeight;

    //This is to give it a little boost when hard avoiding something
    [NativeDisableParallelForRestriction]
    public float HardAvoidanceSpeedMultiplier;
    public float HardAvoidSpeedBoostInRate;
    public float HardAvoidSpeedBoostOutRate;
    
    //---------- Goal Behavior ---------------//
    public bool GoalActive;
    [Unity.Collections.ReadOnly] public float2 FollowTargetPos;
    public float GoalWeight;
    public float FollowDistance;
    public float OrbitKeepDistance;
    public float SwarmRadius;
    public float SwarmRadialWeight;
    public float SwarmRandomWeight;
    public float SwarmNoiseFrequency;
    public float TimeSeconds;
    public float2 CurrentPatrolTarget;
    [Unity.Collections.ReadOnly] public NativeArray<float> OrbitBiasArray; // per agent
    
    //for containment inside circle
    public float2 ContainmentCenter;
    public float ContainmentRadius;
    
    //core variables
    private float2 _selfPos, _selfForward;
    private float2 _alignment, _cohesion, _separation, _avoidanceDir, _goalDir;

    public void Execute(int index)
    {
        _selfPos = Agents[index].Position;
        _selfForward = Agents[index].Forward;

        _alignment = float2.zero;
        _cohesion = float2.zero;
        _separation = float2.zero;
        _avoidanceDir = float2.zero;
        _goalDir = float2.zero;

        int numNeighbors = 0;
        int numSeparation = 0;

        //1. Calculate alignment, cohesion and separation on normal circumstances
        for (int i = 0; i < Agents.Length; i++)
        {
            if (i == index) continue;

            float2 otherPos = Agents[i].Position;
            float2 toOther = otherPos - _selfPos;
            float distSqr = math.lengthsq(toOther);

            if (distSqr <= NeighborDistSqr)
            {
                _alignment += Agents[i].Forward;
                _cohesion += otherPos;
                _separation += ( (_selfPos - otherPos) / distSqr );

                /*if (distSqr < SeparationDist * SeparationDist)
                {
                    _separation += ( (_selfPos - otherPos) / distSqr );
                    numSeparation++;
                }*/

                numNeighbors++;
            }

            if (numNeighbors > MaxNeighbors)
                break;
        }

        if (numNeighbors > 0)
        {
            _alignment = math.normalize(_alignment / numNeighbors) * AlignmentWeight;
            _cohesion = math.normalize((_cohesion / numNeighbors - _selfPos)) * CohesionWeight;
            _separation = math.normalize(_separation / numNeighbors) * SeparationWeight;
        }

        //2. Hard avoidance
        float speedMultiplier = 0;
        if (AvoidActive)
        {
            _avoidanceDir= CalculateHardAvoidDirection(out speedMultiplier);
            
        }
        //3. Goal behaviors
        // This means behaviors that affect final destination of the flock. For now only Patrol and Follow
        // By nature they are mutually exclusive. You cant have both active.
        
        
        if (GoalActive)
        {
            if (ContainmentType != FlockContainmentType.Radius)
            {
                switch (GoalType)
                {
                    case FlockGoalType.Follow:
                        _goalDir = CalculateFollowGoalDir(index);
                        break;
                    case FlockGoalType.Patrol:
                        _goalDir = CalculatePatrolGoalDir();
                        break;
                    
                }
                
            }
            
        }
        //radius containment nullifies everything else
      
        //4. Containment
        float2 baseDir = _alignment + _cohesion + _separation;
        if(math.lengthsq(baseDir) == 0f)
            baseDir = _selfForward;

        float2 finalDir = baseDir + _avoidanceDir + _goalDir;
        finalDir = math.normalizesafe(finalDir, _selfForward);
        Output[index] = finalDir;
        SpeedMultiplierOutput[index] = speedMultiplier;
    }

    private float2 CalculateHardAvoidDirection(out float speedMultiplier)
    {
        speedMultiplier = 1f;
        if (!CanHardAvoid || HardAvoidPositions.Length == 0)
        {
            return float2.zero;
        }

        float2 hardAvoid = float2.zero;
        int hardAvoidCount = 0;

        for (int i = 0; i < HardAvoidPositions.Length; i++)
        {
            float2 toThing = HardAvoidPositions[i] - _selfPos;
            float distToThingSqr = math.lengthsq(toThing);
            if (distToThingSqr >= HardAvoidanceDistanceSqr)
            {
                continue;
            }

            hardAvoid += math.normalizesafe(_selfPos - HardAvoidPositions[i]);
            hardAvoidCount++;
        }

        if (hardAvoidCount == 0)
        {
            return float2.zero;
        }

        speedMultiplier = HardAvoidanceSpeedMultiplier;
        return math.normalizesafe(hardAvoid / hardAvoidCount) * HardAvoidanceWeight;
    }

    private float2 CalculateFollowGoalDir(int index)
    {
        float2 toFollow = FollowTargetPos - _selfPos;
        float sqrDist = math.lengthsq(toFollow);
        float2 toTargetDir = math.normalize(toFollow);

        switch (FollowAfterType)
        {
            case FlockFollowAfterType.Orbit:
                if (OrbitKeepDistance > 0)
                {
                    float dist = math.sqrt(sqrDist);
                    //how far are we?
                    float error = dist - OrbitKeepDistance;

                    //we get the perperdincular vector of that direction. dot product math. For a (x,y) vector then
                    //(-y, x) is 90deg counter-clockwise and (y, -x) is 90deg clockwise
                    float2 perp = new float2(-toTargetDir.y, toTargetDir.x);
                    //This orbit is just a random power so that each fish is a different
                    //tangential (thats a word? idk) force. Otherwise it would like WAY to linear
                    float orbit = 1f + OrbitBiasArray[index] * 0.2f;

                    //push the fish inwardd or outward depending on distance. Radia force
                    float2 maintain = toTargetDir * math.clamp(error, -1f, 1f);
                    //Tangential force
                    float2 orbiting = perp * orbit;

                    return (maintain + orbiting) * GoalWeight;
                }
                break;
            case FlockFollowAfterType.Swarm:
                {
                    float dist = math.sqrt(sqrDist);
                    float keep = math.max(SwarmRadius, 0.01f);

                    // Swarm only inside SwarmKeepDistance.
                    if (sqrDist < SwarmRadius)
                    {
                        //forget about alignmnet, cohesion and separation during swarm
                        _alignment = _cohesion = _separation = float2.zero;
                        
                        float freq = math.max(SwarmNoiseFrequency, 0.1f);
                        float step = math.floor(TimeSeconds * freq);
                        float nx = math.frac(math.sin((index + 1) * 17.123f + step * 13.13f) * 43758.5453f) - 0.5f;
                        float ny = math.frac(math.sin((index + 1) * 91.731f + step * 7.77f) * 24634.6345f) - 0.5f;
                        float2 randomDir = math.normalizesafe(new float2(nx, ny), _selfForward);

                        // Keep a minimum distance without looking like hard evasion.
                        float overlap = math.saturate((keep - dist) / keep);
                        float2 innerCorrection = -toTargetDir * overlap * SwarmRadialWeight;
                        float2 swarmDir = math.normalizesafe((randomDir * SwarmRandomWeight) + innerCorrection, _selfForward);
                        return swarmDir * GoalWeight;
                    }
                   
                }
                break;
        }
        
        //if we got to this point then neither orbit nor swarm are active, so just follow normally
        // Pure follow (at distance)
        if (FollowDistance > 0)
        {
            float dynamicWeight = 0f;
            if (sqrDist <= FollowDistance * FollowDistance)
            {
                // Optional: stronger follow when closer, softer when farther
                dynamicWeight = math.saturate(math.unlerp(FollowDistance * FollowDistance, 1f, sqrDist));
            }
            return math.normalize(toFollow) * dynamicWeight * GoalWeight;
                            
        }
        else
        {
            return math.normalize(toFollow) * GoalWeight;
        }


    }

    /**
     * Patrol can not work if Containment is radius
     */
    private float2 CalculatePatrolGoalDir()
    {
        if (GoalWeight > 0f && ContainmentType != FlockContainmentType.Radius)
        {
            float2 toTarget = math.normalize(CurrentPatrolTarget - _selfPos);
            float align = math.dot(math.normalize(_selfForward), toTarget);

            if (align < 0.8f) // hardcoded alignment threshold
            {
                return toTarget * GoalWeight;
            }
        }
        return float2.zero;
    }

    /**
     * Calculate the direction to go back inside the area if outside the radius.
     * This is done here because it needs to use colliders, no way around that right now
     */
    private float2 CalculateRadiusContainmentDir()
    {
        float2 toCenter = ContainmentCenter - _selfPos;
        float dist = math.length(toCenter);

            
        // Add a small random angular offset so it's not perfectly straight
        /*
        Unity.Mathematics.Random random = new Unity.Mathematics.Random(randomSeeds[index]); // Use Unity.Mathematics.Random
        float randomAngle = (random.NextFloat() - 0.5f) * math.radians(15f);; // e.g. ±15 degrees
        float s = math.sin(randomAngle);
        float c = math.cos(randomAngle);
        float2 rotatedDir = new float2(
            toCenter.x * c - toCenter.y * s,
            toCenter.x * s + toCenter.y * c
        );*/

        if (dist > ContainmentRadius)
        {
            // Direction from agent to center
            float2 containmentDir = math.normalize(ContainmentCenter - _selfPos);
            // Optional: scale by how far outside they are (smooth approach)
            float overflow = dist - ContainmentRadius;
            containmentDir *= overflow; // or math.saturate(overflow / containRadius)
            containmentDir *= 5;
            return containmentDir;
        }
        return float2.zero;
    }
}

[RequireComponent(typeof(FlockCore))]
[BurstCompile]
public class FlockManager : Cullable
{
    [Header("Job")]
    private NativeArray<BoidData> _agentsDataArray;
    private NativeArray<float2> _steeringOutputArray;
    private NativeArray<float> _speedMultiplierOutput;
    private NativeArray<float> _currentSpeedMultiplier;
    private NativeArray<uint> _randomSeeds;
    
    
    
    [Title("Main config")]
    [SerializeField] private bool isActive = true;
    [SerializeField] private FlockBehaviorSO flockBehavior;
    [SerializeField] private FlockAgentJob agentPrefab;
    [SerializeField] private Transform spawnPoint;
    [Range(2, 500)]
    public int numAgents = 250;
    [Range(1f, 100f)]
    public float acceleration = 10f;
    
    // This will automatically pull all tags from the project
    private IEnumerable<string> GetAllTags()
    {
        return UnityEditorInternal.InternalEditorUtility.tags;
    }
    
    
    private FlockBehaviorBase[] _behaviors;
    private readonly List<FlockBehaviorBase> _activeBehaviors = new();
    private readonly List<FlockBehaviorBase> _newlyEnabledExclusiveBehaviors = new();
    private readonly Dictionary<FlockBehaviorBase, bool> _previousBehaviorEnabledState = new();
    private FlockContainment _containment;
    private readonly List<FlockAgentJob> _agents = new List<FlockAgentJob>();
    public Transform[] agentTransforms { get; private set; }

    //Keep tracking
    private Dictionary<Collider2D, FlockAgentJob> _agentColliderMap = new();
    private Vector2 _alignmentDir, _cohesionDir, _separationDir;
    private Transform _agentTransform;
    private Vector3 _currentAgentPos;
    private Vector2 _currentAgentMove;
    private float _currentSqrdDistanceFromPlayer;


    private void Awake()
    {
        // Get all behaviors attached as components
        _behaviors = GetComponents<FlockBehaviorBase>();
        _containment = _behaviors.OfType<FlockContainment>().FirstOrDefault();
      
    }

    
    // Start is called before the first frame update
    void Start()
    {
        SetupFlockJobData();
        InitializeAgents();
    }

    //Necessary setup for flock properties, meaning stuff all agents will share
    private void SetupFlockJobData()
    {
        //initializing native arrays for job
        _agentsDataArray = new NativeArray<BoidData>(numAgents, Allocator.Persistent);
        _steeringOutputArray = new NativeArray<float2>(numAgents, Allocator.Persistent);
        _speedMultiplierOutput = new NativeArray<float>(numAgents, Allocator.Persistent);
        _currentSpeedMultiplier = new NativeArray<float>(numAgents, Allocator.Persistent);
        _randomSeeds = new NativeArray<uint>(numAgents, Allocator.Persistent);

        for (int i = 0; i < numAgents; i++)
        {
            _speedMultiplierOutput[i] = 1f;
            _currentSpeedMultiplier[i] = 1f;
        }
        
    }

    private void InitializeAgents()
    {
        //instantiate agents
        for (int i = 0; i < numAgents; i++)
        {
            FlockAgentJob newAgent = Instantiate(
                agentPrefab,
                (Vector2)spawnPoint.position + (Random.insideUnitCircle * numAgents * 0.08f),
                Quaternion.Euler(Vector3.forward * Random.Range(0f, 360f)),
                transform
            );
            newAgent.name = agentPrefab.name + i;
            newAgent.Initialize(this);
            newAgent.orbitBias = Random.Range(-1f, 1f);
            _agents.Add(newAgent);
            _agentColliderMap[newAgent.GetComponent<Collider2D>()] = newAgent;
        }
        //cache the transforms for faster access
        agentTransforms = _agents.Select(a => a.transform).ToArray();
        
        //init behaviors
        foreach (FlockBehaviorBase behavior in _behaviors)
        {
            behavior.Initialize(this);
        }
    }

    // Update is called once per frame
    void Update()
    {
        if (!isActive || !_agentsDataArray.IsCreated)
            return;

        if (isCulled)
            return;
        
        //1. Basic job data
        for (int i = 0; i < numAgents; i++)
        {
            Transform tf = agentTransforms[i];
            _agentsDataArray[i] = new BoidData
            {
                Position = new float2( tf.position.x,tf.position.y ),
                Forward = new float2( tf.up.x,tf.up.y )
            };
        }

        //2. Create the Job with Common behavior data
        var job = new FlockDirectionJob
        {
            Agents = _agentsDataArray,
            Output = _steeringOutputArray,
            SpeedMultiplierOutput = _speedMultiplierOutput,
            TimeSeconds = Time.time,
            //NeighborDist = flockBehavior.NeighborRadius,
            //SeparationDist = flockBehavior.NeighborRadius * avoidanceRadiusMultiplier,
            
            //for containment inside circle
            //randomSeeds = _randomSeeds,
            
        };

        //3. Calculate behaviors to use
        foreach (FlockBehaviorBase behavior in _behaviors)
        {
            behavior.ApplyToJob(ref job);
        }

        JobHandle handle = job.Schedule(numAgents, 64);
        handle.Complete();
        
        //apply result back to agents
        for (int i = 0; i < numAgents; i++)
        {
            float2 dir = _steeringOutputArray[i];
            float targetSpeedMultiplier = _speedMultiplierOutput[i];
            float currentMultiplier = _currentSpeedMultiplier[i];
            float smoothingRate = targetSpeedMultiplier > currentMultiplier
                ? job.HardAvoidSpeedBoostInRate
                : job.HardAvoidSpeedBoostOutRate;
            currentMultiplier = Mathf.MoveTowards(currentMultiplier, targetSpeedMultiplier, smoothingRate * Time.deltaTime);
            _currentSpeedMultiplier[i] = currentMultiplier;

            float speed = acceleration * currentMultiplier;
            FlockAgentJob agent = _agents[i];
            
            //apply any external forces from the behaviors, like area containment
            foreach (FlockBehaviorBase behavior in _behaviors)
            {
                dir += behavior.GetExternalDirection(ref agent);
            }
            
            dir = math.normalize(dir);
            
            _agents[i].Move(dir * speed);
        }
    }


   

    /*
    private void OnDrawGizmosSelected()
    {
        if (!drawCullingGizmos || !useLeanCulling)
            return;

        if (!TryGetCurrentFlockBounds(out Bounds bounds))
            return;

        Camera cam = _mainCamera != null ? _mainCamera : Camera.main;
        if (cam == null)
            return;

        Vector3 center = bounds.center;

        if (!TryGetCenterToViewportSqrDistance(bounds, out float sqrDist, out Vector3 closestPoint))
            return;

        float maxSqrDist = farCullDistance * farCullDistance;

        Gizmos.color = Color.cyan;
        Gizmos.DrawWireCube(center, bounds.size);

        Gizmos.color = sqrDist <= maxSqrDist ? Color.green : Color.red;
        Gizmos.DrawLine(center, closestPoint);
        Gizmos.DrawSphere(center, 0.15f);
        Gizmos.DrawSphere(closestPoint, 0.12f);
    }*/
    
    private void OnDestroy()
    {
        if (_agentsDataArray.IsCreated) _agentsDataArray.Dispose();
        if (_steeringOutputArray.IsCreated) _steeringOutputArray.Dispose();
        if (_speedMultiplierOutput.IsCreated) _speedMultiplierOutput.Dispose();
        if (_currentSpeedMultiplier.IsCreated) _currentSpeedMultiplier.Dispose();
        if (_randomSeeds.IsCreated) _randomSeeds.Dispose();
    }

    //to toggle the whole acitvity on and off
    public void ToggleActivity(bool newActive)
    {
        this.isActive = newActive;
    }


    public void ToggleHardAvoidance(bool newAvoid)
    {
        //canHardAvoid = newAvoid;
    }
    
    public void ToggleRadiusContainment(bool newContainment)
    {
        if (_containment)
        {
            //_areaContainment.isContainedByRadius = newContainment;
        }
    }


    public bool ToggleGoal(bool newActive)
    {
        var goal = _behaviors.OfType<FlockGoal>().FirstOrDefault();
        if (goal == null) return false;
        goal.isActive = newActive;
        return true;
    }
    
    public bool ToggleContainment(bool newActive)
    {
        var goal = _behaviors.OfType<FlockContainment>().FirstOrDefault();
        if (goal == null) return false;
        goal.isActive = newActive;
        return true;
    }
    
    public bool ToggleGoalType(FlockGoalType newType)
    {
        var goal = _behaviors.OfType<FlockGoal>().FirstOrDefault();
        if (goal == null) return false;
        goal.goalType = newType;
        return true;
    }

    public FlockContainment GetContainmentBehavior()
    {
        return _behaviors.OfType<FlockContainment>().FirstOrDefault();
    }

    #region Culling

    public override Bounds GetCullBounds()
    {
        if (agentTransforms != null && agentTransforms.Length > 0)
        {
            Vector3 startPos = agentTransforms[0].position;
            Bounds bounds = new Bounds(startPos, Vector3.one * 0.5f);
            for (int i = 1; i < agentTransforms.Length; i++)
            {
                bounds.Encapsulate(agentTransforms[i].position);
            }
            return bounds;
        }

        return default;

        // Fallback for editor/debug before runtime arrays are initialized.
        /*if (transform.childCount > 0)
        {
            Vector3 startPos = transform.GetChild(0).position;
            bounds = new Bounds(startPos, Vector3.one * 0.5f);
            for (int i = 1; i < transform.childCount; i++)
            {
                bounds.Encapsulate(transform.GetChild(i).position);
            }
            return true;
        }*/
    }



    public override void SetCulled(bool culled)
    {
        //for flock manager is only just changing a boolean no big deal
        isCulled = culled;
    }

    protected override void OnDrawGizmosSelected()
    {
        //if they are the same it pretty much mean is visible
        if(_cameraClosestPoint == Vector3.one)
            return;

        Bounds c = GetCullBounds();
        Gizmos.color = Color.purple;
        Gizmos.DrawWireCube(c.center, c.size);
        
        Gizmos.color = isCulled ? Color.red : Color.purple;
        Gizmos.DrawLine(c.center, _cameraClosestPoint);
        Gizmos.DrawSphere(c.center, 1f);
        Gizmos.DrawSphere(_cameraClosestPoint, 1f);
        
    }

    #endregion

    
}
