--[[
Copyright 2017-2019 The MCUSim Project.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the MCUSim or its parts nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
--]]

VERBOSE = false			-- Switch on to enable verbose output
TICK_TIME = 0.0			-- clock period, in us

ticks_passed = 0
check_point = 0			-- TCNT0 is 0 at start, count up to 255 then
				-- and reset by overflow interrupt

function module_conf(mcu)
	-- Re-calculate clock period, in us
	TICK_TIME = (1.0/MSIM_Freq(mcu))*1000000.0
	if VERBOSE then
		print("MCU clock: " .. MSIM_Freq(mcu)/1000 .. "kHz")
		print("MCU clock period: " .. TICK_TIME .. "us")
	end
end

function module_tick(mcu)
	if check_point == 0 and AVR_ReadIO(mcu, TCNT0) == 0 then
		check_point = check_point + 1
	elseif check_point == 1 and AVR_ReadIO(mcu, TCNT0) == 255 then
		check_point = check_point + 1
	elseif check_point == 2 and AVR_ReadIO(mcu, TCNT0) == 0 then
		-- Test finished successfully
		MSIM_SetState(mcu, AVR_MSIM_STOP)
		print("ticks passed: " .. ticks_passed)
	elseif ticks_passed > 100000 then
		-- Test failed
		MSIM_SetState(mcu, AVR_MSIM_TESTFAIL)
		print("ticks passed: " .. ticks_passed)
	end

	ticks_passed = ticks_passed + 1
end

