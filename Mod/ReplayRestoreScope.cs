using System;
using System.Collections.Generic;

// A failed renderer/camera restore must never prevent restoring game time or
// input. Main-thread-only; rollback is LIFO and safe to invoke repeatedly.
public sealed class ReplayRestoreScope : IDisposable
{
    private readonly List<Action> undo=new List<Action>();
    public string Error {get;private set;}
    public void Add(Action action){if(action==null)throw new ArgumentNullException("action");undo.Add(action);}
    public void Dispose(){for(int i=undo.Count-1;i>=0;i--)try{undo[i]();}catch(Exception e){if(Error==null)Error=e.Message;}undo.Clear();}
}
