using System;
using System.Collections;
using System.Collections.Generic;
using BehaviorDesigner.Runtime;
using FIMSpace.FTail;
using FMODUnity;
using Pathfinding;
using UnityEngine;

public enum PredatorState {  Default, Investigate, Follow, Chasing, Frustrated };
[Serializable]
public enum PredatorType
{
    Regular,
    Jumper
}

[RequireComponent(typeof(PredatorEyeManager), typeof(StudioEventEmitter),
    typeof(Collider2D))]
public abstract class PredatorBase : Cullable, IPredator
{
    public static event Action<PredatorBase, PredatorState> OnStateChanged;
    public static event Action<PredatorBase> OnPredatorDisabled;
    public static event Action<PredatorBase> OnPredatorEnabled;
    public PredatorState CurrentState { get; protected set; }
    
    
    [Header("Data")]
    [SerializeField] protected PredatorSO predatorData;
    protected IPredatorModule[] _predatorModules;
    private Dictionary<Type, IPredatorModule> _moduleMap;
    
    [Header("Reactions")]
    [SerializeField] protected bool canReactToPlayer; 
    [SerializeField] public bool canAddChaseMusic;
    [HideInInspector]
    public bool isKillingPlayer = false;
    
    [Header("Systems interactions")]
    //If true, then this predator adds to the camera target group and everything
    [SerializeField] public bool canReactToCamera;
    //If true then Predator show visual feedback when Player is close
    [SerializeField] public bool canReactToCloseness;

    protected BehaviorTree BehaviorTree;
    protected AIPath AIPath;
    protected Rigidbody2D Rigidbody2D;
    protected CircleCollider2D MyCollider2D;
    private TailAnimator2[] _tailAnimators;
    
    /**
    * What to do if we receive a signal that the player is near or not
    */
    public abstract void OnCanHearPlayer(bool toggle);
    
    /**
     * What to do when predator actually "hears" the player
     */
    public abstract void OnPredatorHeardPlayer(Vector3 from);

    public abstract void OnEatPlayer(Transform player);
    
    protected virtual void Awake()
    {
        BehaviorTree = GetComponent<BehaviorTree>();
        AIPath = GetComponent<AIPath>();
        
        Rigidbody2D = GetComponent<Rigidbody2D>();
        MyCollider2D = GetComponent<CircleCollider2D>();
        
        //our modular approach! With map this is O(1) to access modules
        _predatorModules = GetComponentsInChildren<IPredatorModule>();
        _moduleMap = new Dictionary<Type, IPredatorModule>();
        foreach (var module in _predatorModules)
            _moduleMap[module.GetType()] = module;

        _tailAnimators = GetComponentsInChildren<TailAnimator2>();
    }

    protected virtual void OnEnable()
    {
        base.OnEnable();
        OnPredatorEnabled?.Invoke(this);
    }

    protected virtual void OnDisable()
    {
        base.OnDisable();
        OnPredatorDisabled?.Invoke(this);
    }

    protected virtual void Start()
    {
        SetupPredator();
    }
    
    public virtual void SetupPredator()
    {
        //first mosnter state update
        if (canReactToPlayer)
        {
            BehaviorTree.SetVariableValue("CanReactToCall",false);
        }
        CurrentState = PredatorState.Default;
        foreach (var module in _predatorModules)
        {
            module.Initialize(predatorData);
            module.OnMonsterStateUpdate(CurrentState);
            
        }
    }
    
    public T GetPredatorModule<T>() where T : class, IPredatorModule
    {
        return _moduleMap.TryGetValue(typeof(T), out var module) ? module as T : null;
    }
    
    public void ToggleBehaviorTree(bool toggle)
    {
        if (BehaviorTree)
        {
            if (toggle)
            {
                AIPath.enabled = true;
                BehaviorTree.enabled = true;
                BehaviorTree.EnableBehavior();
            }
            else
            {
                BehaviorTree.DisableBehavior(true);
                BehaviorTree.enabled = false;
                AIPath.enabled = false;

            }
            
        }
    }
    
      /**
     * Function that updates the monster state visually
     */
    public void UpdateMonsterState(PredatorState newState, bool withSound = true, bool showEffect = false)
    {
        if (CurrentState == newState && CurrentState != PredatorState.Default)
            return;
        //Kill all transitions and return to normal before deciding what to do
        if (CurrentState == PredatorState.Chasing)
        {
            //we were on a chase
        }

        //If the new state is not Investigating then it can not react to Calls
        if (newState != PredatorState.Investigate && canReactToPlayer)
        {
            BehaviorTree.SetVariableValue("CanReactToCall",false);
            //_canReactToCall = true;
        }
        
        switch (newState)
        {
            case PredatorState.Investigate:
                BehaviorTree?.SetVariableValue("CanReactToCall",true);
                break;
        }

        CurrentState = newState;
        OnStateChanged?.Invoke(this,CurrentState);
        foreach (var module in _predatorModules)
            module.OnMonsterStateUpdate(CurrentState);
    }
    

  /**
   * Manual trigger of stimuli. Mainly used on our cinematics really
   */
    public void ManualPlayReaction(bool showEffect, bool animate)
    {
        foreach (var module in _predatorModules)
        {
            module.OnReactToStimuli(showEffect,animate);
        }
    }
    
    /**
     * Setup patrols potins for the behavior
     * Easier to just pass an object and then get all the child transforms
     */
    public void SetupPatrolPoints(Transform patrolObj)
    {
        if (patrolObj)
        {
            List<GameObject> patrolPoints = new List<GameObject>();
            foreach (Transform child in patrolObj)
            {
                patrolPoints.Add(child.gameObject);
            }
            BehaviorTree.SetVariableValue("PatrolInfo",patrolPoints);
        }
    }

#region DynamicCulling

    public override Bounds GetCullBounds() => MyCollider2D.bounds;

    public override void SetCulled(bool culled)
    {
        isCulled = culled;
        ToggleBehaviorTree(!culled);
        foreach (var tail in _tailAnimators)
            tail.enabled = !culled;
    }
    
#endregion

#if UNITY_EDITOR
    [Header("Debug")]
    [SerializeField] private PredatorState debugState = PredatorState.Default;
    private PredatorState _lastDebugState = PredatorState.Default;

    private void OnValidate()
    {
        if (!Application.isPlaying) return;
        if (debugState == _lastDebugState) return;

        _lastDebugState = debugState;
        UpdateMonsterState(debugState);
    }
    protected virtual void OnDrawGizmosSelected()
    {
        if (drawCullingGizmos)
        {
            Camera cam = Camera.main;
            
            Vector3 center = transform.position;

            if (cam == null)
                return;

            Vector3 camPos = cam.transform.position;
            float sqrDist = (center - camPos).sqrMagnitude;
            float maxSqrDist = cullDistance * cullDistance;

            Gizmos.color = sqrDist <= maxSqrDist ? Color.green : Color.red;
            Gizmos.DrawLine(center, camPos);
            //Gizmos.DrawWireSphere(camPos, cullDistance);
        }


    }
#endif


    
}

