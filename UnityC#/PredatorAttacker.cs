using System;
using System.Collections;
using System.Collections.Generic;
using DG.Tweening;
using FMODUnity;
using Sirenix.OdinInspector;
using UnityEngine;
using UnityEngine.Serialization;


/**
 * This type of predator can hear and react to player calls, follow and chase the player
 * It uses a behavior tree to manage its states
 */
public class PredatorAttacker : PredatorBase
{
    [SerializeField] private ParticleSystem vfxNearPlayer;
    
    [SerializeField] private bool isPatroler = true;
    [ShowIf(nameof(isPatroler))]
    [SerializeField] protected Transform patrolObject;
    
    protected StudioEventEmitter MonsterChaseMusicEmitter;
    

    protected override void Awake()
    {
        base.Awake();
        MonsterChaseMusicEmitter = GetComponent<StudioEventEmitter>();
    }

    protected override void OnEnable()
    {
        base.OnEnable();
        PlayerCharacter.OnPlayerHide += OnPlayerHides;
    }

    protected override void OnDisable()
    {
        base.OnDisable();
        PlayerCharacter.OnPlayerHide -= OnPlayerHides;
    }

    public override void SetupPredator()
    {
        base.SetupPredator();
        
        //setup patrol points
        SetupPatrolPoints(patrolObject);
        
        BehaviorTree.SetVariableValue("isPatrolType",isPatroler);
        BehaviorTree.SetVariableValue("PatrolSpeed",predatorData.PatrolSpeed);
        //BehaviorTree.SetVariableValue("playerRef",GameManager.PlayerTransform.gameObject);
        if (canReactToPlayer )
        {
            BehaviorTree.SetVariableValue("FollowSpeed",predatorData.FollowSpeed);
            BehaviorTree.SetVariableValue("FollowRange",predatorData.FollowRange);
            BehaviorTree.SetVariableValue("HiddenFollowRange",predatorData.FollowRange / 2);
            BehaviorTree.SetVariableValue("ChasingSpeed",predatorData.ChasingSpeed);
            BehaviorTree.SetVariableValue("ChasingRange",predatorData.ChasingRange);
            BehaviorTree.SetVariableValue("HiddenChasingRange",predatorData.ChasingRange / 2);

            if (GameManager.Instance)
            {
                BehaviorTree.SetVariableValue("playerLastPosition",GameManager.Instance.playerLastPosition);
            }
        }
    }

    /**
     * When this predator hears the player
     */
    public override void OnPredatorHeardPlayer(Vector3 from)
    {
        //able to react?
        if (canReactToPlayer && CurrentState != PredatorState.Chasing && CurrentState != PredatorState.Follow)
        {
            StartCoroutine(ReactToPlayerCall());
        }
    }
    
    public override void OnEatPlayer(Transform player)
    {
        if (canReactToPlayer)
        {
            isKillingPlayer = true;
            BehaviorTree.SetVariableValue("isChasing",false);
            BehaviorTree.SetVariableValue("isFollowing",false);
            ToggleBehaviorTree(false);
            Rigidbody2D.AddForce(transform.up * 20f, ForceMode2D.Impulse);
            
            foreach (var module in _predatorModules)
            {
                module.OnEatPlayer(player);
            }
            DOVirtual.DelayedCall(2f, () =>
            {
                ToggleBehaviorTree(true);
                isKillingPlayer = false;
            });
        }
    }
    
    public IEnumerator ReactToPlayerCall()
    {
        yield return new WaitForSeconds(.5f);
        UpdateMonsterState(PredatorState.Investigate);
    }

    public bool CanReactToPlayer()
    {
        return canReactToPlayer;
    }
    

    //react to playerCharacter call, go investigate a position using a lastpos object since btree needs an object
    //Fire when entering chase mode
    protected void EnterChaseMode()
    {
        Debug.Log("chase mode sfx");
        MonsterChaseMusicEmitter.Play();
    }

    private void OnPlayerHides()
    {
        if (CurrentState == PredatorState.Chasing ||
            CurrentState == PredatorState.Follow)
        {
            //player hides when we were chasing or following. Stop that right away
            BehaviorTree.SetVariableValue("isChasing",false);
            BehaviorTree.SetVariableValue("isFollowing",false);
            BehaviorTree.SendEvent("PlayerHidesDuringChase");
            UpdateMonsterState(PredatorState.Frustrated);
        }
    }

    
    
#region BehaviourTreeEvents
    public void OnAIReactToCall()
    {
        //only if we are not already chasing or following a playerCharacter
        UpdateMonsterState(PredatorState.Investigate);
    }
    //AI tree begins following playerCharacter
    public void OnAINoticePlayer()
    {
        UpdateMonsterState(PredatorState.Follow);
    }
    
    //AI tree begins chasing playerCharacter
    public void OnAIChasePlayer()
    {
        UpdateMonsterState(PredatorState.Chasing);
    }

    //AI tree goes back to patrol
    public void OnAIBackToPatrol()
    {
        BehaviorTree.SetVariableValue("isFollowing",false);
        BehaviorTree.SetVariableValue("isChasing",false);
        UpdateMonsterState(PredatorState.Default);
        ToggleBehaviorTree(true);
    }
    
#endregion



    /**
     * When this predator can hear the player and should send some feedback
     */
    public override void OnCanHearPlayer(bool toggle)
    {
        if (CurrentState != PredatorState.Chasing && CurrentState != PredatorState.Follow)
        {
            if (toggle && canReactToCloseness)
            {
                if(!vfxNearPlayer.isPlaying)
                    vfxNearPlayer.Play();
            }
            else if(vfxNearPlayer.isPlaying)
            {
                Debug.Log("Stop near vfx");
                vfxNearPlayer.Stop();
            }
            
        }
    }
    
    //show distance on camera
    protected override void OnDrawGizmosSelected()
    {
        base.OnDrawGizmosSelected();
        if(canReactToPlayer)
        {
            Gizmos.color = Color.red;
            Gizmos.DrawWireSphere(transform.position, predatorData.ChasingRange);
            Gizmos.color = Color.yellow;
            Gizmos.DrawWireSphere(transform.position, predatorData.FollowRange);
            
        }
    }

}

