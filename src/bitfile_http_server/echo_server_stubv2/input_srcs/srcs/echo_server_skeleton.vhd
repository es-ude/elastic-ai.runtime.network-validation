LIBRARY ieee;
USE ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library xil_defaultlib;

entity echo_server_skeleton is
    port (
        -- control interface
        clock                : in std_logic;
        clk_hadamard                : in std_logic;
        reset                : in std_logic; -- controls functionality (sleep)
        busy                : out std_logic; -- done with entire calculation
        wake_up             : out std_logic;        
        -- indicate new data or request
        rd                    : in std_logic;    -- request a variable
        wr                 : in std_logic;     -- request changing a variable
        
        -- data interface
        data_in            : in std_logic_vector(7 downto 0);
        address_in        : in std_logic_vector(15 downto 0);
        data_out            : out std_logic_vector(7 downto 0);
        
        debug                : out std_logic_vector(7 downto 0);
        
        led_ctrl             : out std_logic_vector(3 DOWNTO 0)
    );
end echo_server_skeleton;

architecture rtl of echo_server_skeleton is

    type buf_t is array (0 to 15) of std_logic_vector(7 downto 0);
    signal data_buf : buf_t := (x"ee", x"dd", x"cc", x"bb", x"aa", x"99", x"88", x"77", x"66", x"55", x"44", x"33", x"22", x"11", others=>(x"00"));

    signal skeleten_id_str : buf_t := (x"32", x"33", x"31", x"30", x"30", x"39", x"65", x"63", x"68", x"6F", x"73", x"65", x"72", x"76", x"65", x"72");

begin

    -- not used in this design
    busy <= '0';
    wake_up <= '0';

    -- process data receive 
    process (clock, rd, wr, reset)
        variable int_addr : integer range 0 to 20000;
    begin
        
        if reset = '0' then
            if rising_edge(clock) then
                -- process address of written value
                if wr = '1' or rd = '1' then
                    int_addr := to_integer(unsigned(address_in));
                    if wr = '1' then
                        if int_addr>=16 then
                            data_buf(int_addr-16) <= data_in(7 downto 0);
                        end if;
                    elsif rd = '1' then
                        if int_addr<=15 then
                            data_out(7 downto 0) <= skeleten_id_str(int_addr);
                        elsif int_addr>=16 then
                            data_out(7 downto 0)<= data_buf(int_addr-16);
                        end if;
                    end if;
                end if;
            end if;
        end if;
    end process;
end rtl;