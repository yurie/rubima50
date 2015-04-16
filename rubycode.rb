class RubyConf
  def initialize(year, venue)
    @year = year
    @venue = venue
  end

  def prev
    @year = 2005
  end
end

v = 2 + 3
num = 1
num = v + num

v = "mruby"
v2 =  :cruby
matz = {created: [v, v2]}
p matz

c = RubyConf.new(2014, "San Diego")
p c
c.prev
p c

