local h={a=1,b=2}
for i=1,10000 do
  h[string.format("key%05d",i)]=i
end
for i=1,10000000 do
  h.a,h.b=h.b,h.a
end
--print("a=",h.a,"b=",h.b)
