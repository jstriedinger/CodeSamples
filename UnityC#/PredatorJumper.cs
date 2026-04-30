using System.Collections;
using DG.Tweening;
using UnityEngine;

public class PredatorJumper : PredatorAttacker
{
    [Header("Jumper Settings")]
    [SerializeField] private ParticleSystem vfxBubblesLaunch;
    [SerializeField] private float launchForce;
    [SerializeField] private float timeBetweenLaunch;
    [SerializeField] private float launchRadius;
    private float _nextLaunch;
    private float _savedLinearDamping;
    private Tween _resetAITween;

    private void FixedUpdate()
    {
        DoJumpAttack();
    }

    private void DoJumpAttack()
    {
        if (CurrentState != PredatorState.Chasing || !canReactToPlayer)
            return;

        if (Time.time < _nextLaunch)
            return;

        float sqrDistance = (GameManager.PlayerTransform.position - transform.position).sqrMagnitude;
        if (sqrDistance > launchRadius * launchRadius)
            return;

        _resetAITween?.Kill();
        _resetAITween = null;

        // Disable behavior tree so it stops issuing movement commands
        BehaviorTree.DisableBehavior(true);
        BehaviorTree.enabled = false;
        // simulateMovement=false tells A* to skip FinalizeMovement/MovePosition next tick,
        // without removing itself from BatchedEvents (avoids the component enable/disable race)
        AIPath.simulateMovement = false;

        Vector2 dir = (GameManager.PlayerTransform.position - transform.position).normalized;
        float angle = Mathf.Atan2(dir.y, dir.x) * Mathf.Rad2Deg - 90f;
        Rigidbody2D.SetRotation(angle);

        _savedLinearDamping = Rigidbody2D.linearDamping;
        Rigidbody2D.linearDamping = 0f;

        StartCoroutine(ApplyLaunch(dir));
        _nextLaunch = Time.time + timeBetweenLaunch;
    }

    // Waits one fixed frame so any MovePosition A* already queued this frame clears
    // through physics, then applies force in a clean physics step.
    private IEnumerator ApplyLaunch(Vector2 dir)
    {
        yield return new WaitForFixedUpdate();
        Rigidbody2D.AddForce(dir * launchForce, ForceMode2D.Impulse);
        vfxBubblesLaunch.Play();

        _resetAITween = DOVirtual.DelayedCall(timeBetweenLaunch + 0.1f, () =>
        {
            Rigidbody2D.linearDamping = _savedLinearDamping;
            if (!isKillingPlayer)
            {
                AIPath.simulateMovement = true;
                BehaviorTree.enabled = true;
                BehaviorTree.EnableBehavior();
            }
        }).SetLink(gameObject);
    }
}
