using System;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using UnityEngine;

public static class InspectorFields
{
    private static readonly CultureInfo C=CultureInfo.InvariantCulture;
    private static readonly Dictionary<Type,FieldInfo[]> cache=new Dictionary<Type,FieldInfo[]>();
    public static string Number(float v){return v.ToString("G9",C);}
    public static string Vector(Vector3 v){return Number(v.x)+", "+Number(v.y)+", "+Number(v.z);}
    public static string Describe(object value){
        if(ReferenceEquals(value,null))return "null";
        if(value is bool)return (bool)value?"true":"false";
        if(value is string){string s=(string)value;return s.Length>384?s.Substring(0,384)+"...":s;}
        Type t=value.GetType();
        if(t.IsEnum){string name=Enum.GetName(t,value);return (name??"unnamed")+" ("+Enum.Format(t,value,"D")+")";}
        if(value is float)return Number((float)value);
        if(value is double)return ((double)value).ToString("G17",C);
        if(value is byte||value is sbyte||value is short||value is ushort||value is int||value is uint||value is long||value is ulong||value is char||value is decimal)return Convert.ToString(value,C);
        if(value is Vector3)return Vector((Vector3)value);
        if(value is Vector2){var v=(Vector2)value;return Number(v.x)+", "+Number(v.y);}
        if(value is Quaternion){var q=(Quaternion)value;return Number(q.x)+", "+Number(q.y)+", "+Number(q.z)+", "+Number(q.w);}
        if(value is Color){var c=(Color)value;return Number(c.r)+", "+Number(c.g)+", "+Number(c.b)+", "+Number(c.a);}
        var array=value as Array;if(array!=null)return t.GetElementType().Name+"["+array.Length.ToString(C)+"] (not expanded)";
        var obj=value as UnityEngine.Object;if(!ReferenceEquals(obj,null))return obj==null?"Destroyed "+t.Name:t.Name+" #"+obj.GetInstanceID().ToString(C)+" "+obj.name;
        // Never invoke a game's arbitrary ToString override or property getter.
        return t.Name+" (not expanded)";
    }
    public static FieldInfo[] Readable(Type type,Assembly allowed=null){
        bool standard=allowed==null;if(standard&&cache.ContainsKey(type))return cache[type];allowed=allowed??typeof(MouseLook).Assembly;
        var output=new List<FieldInfo>();
        for(var at=type;at!=null&&at.Assembly==allowed&&output.Count<256;at=at.BaseType){
            var fields=at.GetFields(BindingFlags.Instance|BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.DeclaredOnly);Array.Sort(fields,(a,b)=>StringComparer.Ordinal.Compare(a.Name,b.Name));
            foreach(var field in fields){if(output.Count==256)break;if(field.IsStatic||field.IsLiteral||field.FieldType.IsPointer||field.FieldType.IsByRef)continue;output.Add(field);}
        }
        var result=output.ToArray();if(standard&&cache.Count<128)cache[type]=result;return result;
    }
}
