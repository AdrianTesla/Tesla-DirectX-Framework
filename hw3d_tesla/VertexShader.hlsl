struct VSOut
{
    float2 tc  : TexCoord;
    float4 pos : SV_Position;
};

VSOut main(float2 pos : Position, float2 tc : TexCoord)
{
    VSOut v;
  
    v.pos = float4(pos.x, pos.y, 0.0f, 1.0f);
    v.tc  = tc;
    
    return v;
}