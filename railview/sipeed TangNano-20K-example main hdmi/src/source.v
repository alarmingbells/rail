
module frame_buffer (input clk,
                    input [12:0] address,
                    input [12:0] addr_internal,
                    input [1:0] colour,
                    input IE,
                    output reg [1:0] dataOut,
                    output reg [23:0] bgcolour);
    reg [1:0] buffer [1 << 13:0];


    always @(posedge clk) begin
        if (!IE) 
            buffer[address] <= colour;

        bgcolour[7:0] <= {buffer[13'h1FFF], buffer[13'h1FFD], buffer[13'h1FFC], buffer[13'h1FFB]};
        bgcolour[15:8] <= {buffer[13'h1FFA], buffer[13'h1FF9], buffer[13'h1FF8], buffer[13'h1FF7]};
        bgcolour[23:16] <= {buffer[13'h1FF6], buffer[13'h1FF5], buffer[13'h1FF4], buffer[13'h1FF3]};

        dataOut <= buffer[addr_internal];
    end
    
endmodule

module palette_sw (output reg [1:0] palette,
                    input clk,
                    input sw); 

    reg [19:0] debounce;

    always @(posedge clk) begin
        if (debounce > 20'd0 & sw) 
            debounce <= debounce -1'b1;
        else if (!sw) 
            debounce <= 20'd1_000_000; 
    end
    
    always @(negedge sw) begin
        if (debounce == 20'd0) begin
            case (palette)
                2'b00 : palette <= 2'b01;
                2'b01 : palette <= 2'b10;
                2'b10 : palette <= 2'b11;
                2'b11 : palette <= 2'b00;
                default : palette <= 2'b00;
            endcase 
        end
    end

endmodule