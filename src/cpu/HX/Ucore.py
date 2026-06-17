from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject
from m5.objects.TickedObject import TickedObject

class Ucore(TickedObject):
    type="Ucore"
    cxx_header="cpu/HX/Ucore.hh"
    cxx_class="gem5::Ucore"

    