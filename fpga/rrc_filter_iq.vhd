--------------------------------------------------------------------------------
-- rrc_filter_iq.vhd
-- I/Q Wrapper for RRC Filter - PlutoSDR FPGA Integration
--
-- COSPAS-SARSAT T.018 OQPSK Modulation
--
-- This wrapper instantiates two RRC filters (one for I, one for Q)
-- and provides a clean interface for integration into the PlutoSDR design.
--
-- Features:
--   - Dual-channel filtering (I and Q synchronized)
--   - Single control interface
--   - Automatic synchronization of both channels
--   - Ready signal combines both I and Q valid outputs
--
-- Author: SARSAT_SGB Project
-- Date: 2025-10-18
--------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity rrc_filter_iq is
    Port (
        -- Clock and control
        clk         : in  std_logic;                        -- System clock
        reset       : in  std_logic;                        -- Synchronous reset
        enable      : in  std_logic;                        -- Filter enable

        -- I channel input/output
        i_data_in   : in  signed(11 downto 0);              -- I input (Q11)
        i_data_out  : out signed(11 downto 0);              -- I output (Q11)

        -- Q channel input/output
        q_data_in   : in  signed(11 downto 0);              -- Q input (Q11)
        q_data_out  : out signed(11 downto 0);              -- Q output (Q11)

        -- Control signals
        data_valid  : in  std_logic;                        -- Input data valid
        data_ready  : out std_logic                         -- Output data ready
    );
end rrc_filter_iq;

architecture Behavioral of rrc_filter_iq is

    -- Component declaration for RRC filter
    component rrc_filter is
        Port (
            clk         : in  std_logic;
            reset       : in  std_logic;
            enable      : in  std_logic;
            data_in     : in  signed(11 downto 0);
            data_valid  : in  std_logic;
            data_out    : out signed(11 downto 0);
            data_ready  : out std_logic
        );
    end component;

    -- Internal signals
    signal i_ready : std_logic;
    signal q_ready : std_logic;
    signal combined_ready : std_logic;

begin

    -- I-channel RRC filter instance
    rrc_i : rrc_filter
        port map (
            clk         => clk,
            reset       => reset,
            enable      => enable,
            data_in     => i_data_in,
            data_valid  => data_valid,
            data_out    => i_data_out,
            data_ready  => i_ready
        );

    -- Q-channel RRC filter instance
    rrc_q : rrc_filter
        port map (
            clk         => clk,
            reset       => reset,
            enable      => enable,
            data_in     => q_data_in,
            data_valid  => data_valid,
            data_out    => q_data_out,
            data_ready  => q_ready
        );

    -- Combine ready signals (both I and Q must be ready)
    combined_ready <= i_ready and q_ready;
    data_ready <= combined_ready;

end Behavioral;
