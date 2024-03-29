package hcl

/*
#include <hcl.h>
*/
import "C"
import (
	"sync"
)

type Instance struct {
	c *C.hcl_t // c object
	g *HCL     // go object
}

type InstanceTable struct {
	mtx        sync.Mutex
	insts      []Instance
	free_slots []int
}

func (itab *InstanceTable) add_instance(c *C.hcl_t, g *HCL) int {
	itab.mtx.Lock()
	defer itab.mtx.Unlock()

	var n int = len(itab.free_slots)

	if n <= 0 { // no free slots
		itab.insts = append(itab.insts, Instance{c: c, g: g})
		return len(itab.insts) - 1
	} else {
		var slot int
		n--
		slot = itab.free_slots[n]
		itab.free_slots = itab.free_slots[:n]
		itab.insts[slot].c = c
		return slot
	}
}

func (itab *InstanceTable) delete_instance(slot int) Instance {
	var (
		h Instance
		n int
	)

	itab.mtx.Lock()
	defer itab.mtx.Unlock()

	h = itab.insts[slot]
	itab.insts[slot].c = nil
	itab.insts[slot].g = nil

	n = len(itab.insts)
	if slot == n-1 {
		itab.insts = itab.insts[:n-1]
	} else {
		itab.free_slots = append(itab.free_slots, slot)
	}

	return h
}

func (itab *InstanceTable) slot_to_instance(slot int) Instance {
	itab.mtx.Lock()
	defer itab.mtx.Unlock()
	return itab.insts[slot]
}
