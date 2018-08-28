#!/usr/bin/perl
use strict;
use Time::HiRes;

my $tests={
  fib=>{
    zorro=>'fib.zs',
    lua=>'fib.lua',
    python=>'fib.py',
  },
  wloop=>{
    zorro=>'wloop.zs',
    lua=>'wloop.lua',
    python=>'wloop.py',
  },
  floop=>{
    zorro=>'floop.zs',
    lua=>'floop.lua',
    python=>'floop.py',
  },
  iterarray=>{
    zorro=>'iterarray.zs',
    lua=>'iterarray.lua',
    python=>'iterarray.py',
  },
  itermap=>{
    zorro=>'itermap.zs',
    lua=>'itermap.lua',
    python=>'itermap.py'
  },
  closure=>{
    zorro=>'closure.zs',
    lua=>'closure.lua',
    python=>'closure.py'
  },
  strjoin=>{
    zorro=>'join.zs',
    lua=>'join.lua',
    python=>'join.py'
  },
  methods=>{
    zorro=>'methods.zs',
    lua=>'methods.lua',
    python=>'methods.py'
  },
  hashswap=>{
    zorro=>'hashswap.zs',
    lua=>'hashswap.lua',
    python=>'hashswap.py'
  }
};

my @lng=qw(lua zorro python);


my %lnch;
if($^O eq 'MSWin32')
{
%lnch=(
  zorro=>'..\\..\\build\\zorro.exe',
  lua=>'C:\\Work\\cpp\\lua\\lua-5.3.5\\src\\lua.exe',
  python=>'C:\\Soft\\Python35\\python.exe',
);
}else
{
%lnch=(
  zorro=>'../../build/zorro',
  lua=>'lua',
  python=>'python3',
);
}

my $result=[];
my $mxt=0;
$|=1;
print "Running:";
for my $t(keys(%$tests))
{
  print ".";
  $mxt=length($t) if length($t)>$mxt;
  my $curres={};
  for my $l(@lng)
  {
    next unless exists($tests->{$t}->{$l});
    my $bin=$lnch{$l};
    my $scr=$tests->{$t}->{$l};
    my $res=exectest($bin,$scr);
    $curres->{$l}=$res;
  }
  push @$result,{name=>$t,results=>$curres};
}
print "\n";

my $w={};
for my $l(@lng)
{
  my $mx=length($l);
  for my $r(@$result)
  {
    next unless exists($r->{$l});
    my $len=length(sprintf("%.2f",$r->{$l}));
    $mx=$len if $len>$mx;
  }
  $w->{$l}=$mx;
}

print ' 'x$mxt.' ';
for my $l(@lng)
{
  print ' 'x($w->{$l}+1-length($l));
  print $l;
}
print "\n";
for my $r(@$result)
{
  printf("%*s ",$mxt,$r->{name});
  for my $l(@lng)
  {
    if(exists($r->{results}->{$l}))
    {
      my $str=sprintf("%.2f",$r->{results}->{$l});
      print(' 'x(1+$w->{$l}-length($str)));
      print $str;
    }else
    {
      print(' 'x($w->{$l}+1));
    }
  }
  print "\n";
}

sub exectest{
  my ($launcher,$script)=@_;
  my $t1=Time::HiRes::time;
  my $null=$^O eq 'MSWin32'?'nul':'/dev/null';
  system("$launcher $script >".$null);
  my $t2=Time::HiRes::time;
  #return $t2[2]+$t2[3]-$t[2]-$t[3];
  return $t2-$t1;
}
