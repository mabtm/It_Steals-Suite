using System;
using UnityEngine;

public static class CatalogOrder
{
    // Unity tag lookup order is not a stable seed identifier. Only explicit
    // Practice replays align the new objects with the captured catalog.
    public static int[] Match(Vector3[] expected,Vector3[] actual)
    {
        if(expected==null||actual==null||expected.Length!=actual.Length)return null;
        var used=new bool[actual.Length];var indices=new int[expected.Length];
        for(int i=0;i<expected.Length;i++) {
            int found=-1;
            for(int j=0;j<actual.Length;j++)if(!used[j]&&(expected[i]-actual[j]).sqrMagnitude<=.000001f){found=j;break;}
            if(found<0)return null;indices[i]=found;used[found]=true;
        }
        return indices;
    }
}
