using UnityEngine;


public interface IPredator
{
    void SetupPredator();
}
public interface IPredatorModule
{
    void OnMonsterStateUpdate(PredatorState state);

    void Initialize(PredatorSO data);

    void OnEatPlayer(Transform player);
    
    void OnReactToStimuli(bool showEffect, bool animate);
}
