--------------------------------------------------------------------------------
-- rrc_filter.vhd
-- Root-Raised Cosine FIR Filter for PlutoSDR FPGA
--
-- COSPAS-SARSAT T.018 OQPSK Modulation
--
-- Parameters:
--   - Filter taps: 63
--   - Roll-off (α): 0.8
--   - Input: 12-bit signed (Q11 format from buffer)
--   - Output: 12-bit signed (Q11 format to AD9363 DAC)
--   - Coefficients: 16-bit signed (Q15 format)
--
-- Architecture:
--   - FIR filter with 63 taps
--   - Shift register for input samples
--   - Parallel multiply-accumulate (MAC)
--   - Saturation logic for output
--
-- Author: SARSAT_SGB Project
-- Date: 2025-10-18
--------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity rrc_filter is
    Port (
        clk         : in  std_logic;                        -- System clock (typically 100 MHz)
        reset       : in  std_logic;                        -- Synchronous reset
        enable      : in  std_logic;                        -- Filter enable
        data_in     : in  signed(11 downto 0);              -- Input sample (Q11)
        data_valid  : in  std_logic;                        -- Input data valid strobe
        data_out    : out signed(11 downto 0);              -- Output sample (Q11)
        data_ready  : out std_logic                         -- Output data ready
    );
end rrc_filter;

architecture Behavioral of rrc_filter is

    -- Constants
    constant NUM_TAPS : integer := 63;

    -- RRC coefficients (Q15 format)
    type coeff_array is array (0 to NUM_TAPS-1) of signed(15 downto 0);
    constant RRC_COEFFS : coeff_array := (
        0  => to_signed( 32767, 16),  1  => to_signed( -2943, 16),
        2  => to_signed(  -314, 16),  3  => to_signed(    63, 16),
        4  => to_signed(   128, 16),  5  => to_signed(   107, 16),
        6  => to_signed(    63, 16),  7  => to_signed(    19, 16),
        8  => to_signed(   -11, 16),  9  => to_signed(   -26, 16),
        10 => to_signed(   -27, 16),  11 => to_signed(   -18, 16),
        12 => to_signed(    -6, 16),  13 => to_signed(     5, 16),
        14 => to_signed(    11, 16),  15 => to_signed(    12, 16),
        16 => to_signed(     9, 16),  17 => to_signed(     3, 16),
        18 => to_signed(    -2, 16),  19 => to_signed(    -6, 16),
        20 => to_signed(    -7, 16),  21 => to_signed(    -5, 16),
        22 => to_signed(    -2, 16),  23 => to_signed(     1, 16),
        24 => to_signed(     4, 16),  25 => to_signed(     4, 16),
        26 => to_signed(     3, 16),  27 => to_signed(     1, 16),
        28 => to_signed(    -1, 16),  29 => to_signed(    -3, 16),
        30 => to_signed(    -3, 16),  31 => to_signed(    -2, 16),
        32 => to_signed(    -1, 16),  33 => to_signed(     1, 16),
        34 => to_signed(     2, 16),  35 => to_signed(     2, 16),
        36 => to_signed(     2, 16),  37 => to_signed(     1, 16),
        38 => to_signed(    -1, 16),  39 => to_signed(    -1, 16),
        40 => to_signed(    -2, 16),  41 => to_signed(    -1, 16),
        42 => to_signed(     0, 16),  43 => to_signed(     0, 16),
        44 => to_signed(     1, 16),  45 => to_signed(     1, 16),
        46 => to_signed(     1, 16),  47 => to_signed(     0, 16),
        48 => to_signed(     0, 16),  49 => to_signed(    -1, 16),
        50 => to_signed(    -1, 16),  51 => to_signed(    -1, 16),
        52 => to_signed(     0, 16),  53 => to_signed(     0, 16),
        54 => to_signed(     1, 16),  55 => to_signed(     1, 16),
        56 => to_signed(     1, 16),  57 => to_signed(     0, 16),
        58 => to_signed(     0, 16),  59 => to_signed(    -1, 16),
        60 => to_signed(    -1, 16),  61 => to_signed(    -1, 16),
        62 => to_signed(     0, 16)
    );

    -- Shift register for input samples (12-bit)
    type shift_reg_array is array (0 to NUM_TAPS-1) of signed(11 downto 0);
    signal shift_reg : shift_reg_array := (others => (others => '0'));

    -- Accumulator (Q26 = Q11 * Q15, with headroom)
    signal accumulator : signed(31 downto 0);

    -- Output register
    signal output_reg : signed(11 downto 0);
    signal output_valid : std_logic;

    -- Pipeline control
    signal compute_done : std_logic;

begin

    -- Main FIR filter process
    process(clk)
        variable temp_acc : signed(31 downto 0);
        variable product : signed(27 downto 0);  -- Q11 * Q15 = Q26
    begin
        if rising_edge(clk) then
            if reset = '1' then
                -- Reset shift register
                shift_reg <= (others => (others => '0'));
                accumulator <= (others => '0');
                output_reg <= (others => '0');
                output_valid <= '0';
                compute_done <= '0';

            elsif enable = '1' then
                if data_valid = '1' then
                    -- Shift in new sample
                    for i in NUM_TAPS-1 downto 1 loop
                        shift_reg(i) <= shift_reg(i-1);
                    end loop;
                    shift_reg(0) <= data_in;

                    -- Compute convolution (MAC operation)
                    temp_acc := (others => '0');
                    for i in 0 to NUM_TAPS-1 loop
                        -- Multiply: Q11 * Q15 = Q26
                        product := shift_reg(i) * RRC_COEFFS(i);
                        -- Accumulate
                        temp_acc := temp_acc + resize(product, 32);
                    end loop;

                    accumulator <= temp_acc;
                    compute_done <= '1';

                elsif compute_done = '1' then
                    -- Scale back from Q26 to Q11 (divide by 2^15)
                    -- Extract bits [26:15] and saturate to 12-bit
                    if accumulator(31) = '1' then
                        -- Negative: check for underflow
                        if accumulator(31 downto 26) /= "111111" then
                            output_reg <= to_signed(-2048, 12);  -- Saturate to min
                        else
                            output_reg <= accumulator(26 downto 15);
                        end if;
                    else
                        -- Positive: check for overflow
                        if accumulator(31 downto 26) /= "000000" then
                            output_reg <= to_signed(2047, 12);   -- Saturate to max
                        else
                            output_reg <= accumulator(26 downto 15);
                        end if;
                    end if;

                    output_valid <= '1';
                    compute_done <= '0';

                else
                    output_valid <= '0';
                end if;
            else
                output_valid <= '0';
            end if;
        end if;
    end process;

    -- Output assignment
    data_out <= output_reg;
    data_ready <= output_valid;

end Behavioral;
