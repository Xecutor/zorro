//return function
func f()
  return !5+5+5
end

func g()
  x=0..5
  for q in x
    print("q=",q)
    print(f()()+f()())
    if q==3
      print("three")
    end
  end
end

g()
