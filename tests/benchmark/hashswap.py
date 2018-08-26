def f():
  h={}
  h['a']=1
  h['b']=2
  for i in range(0,10000):
    h["key%05d"%i]=i
  for i in range(0,10000000):
    h['a'],h['b']=h['b'],h['a']
  print("a=%d,b=%d\n"%(h['a'],h['b']))

f()
