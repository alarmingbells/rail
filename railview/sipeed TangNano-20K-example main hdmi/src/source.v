
module frame_buffer (input clk,
                    input [12:0] address,
                    input [12:0] addr_internal,
                    input [1:0] colour,
                    input IE,
                    output reg [1:0] dataOut,
                    output reg [23:0] bgcolour);
    reg [1:0] buffer [0:8191];
    reg writing;
    reg [2:0] delayClocks;
    reg IEp0;
    reg IEp1;
    reg [12:0] addressLatch;
    reg [1:0] datap0;
    reg [1:0] datap1;

    wire ieFall = IEp1 & ~IEp0;
    wire ieRise = ~IEp1 & IEp0;

    always @(posedge clk) begin
        IEp0 <= IE;
        IEp1 <= IEp0;

        if (delayClocks > 3'h0) begin
            delayClocks <= delayClocks - 3'h1;
        end

        if (ieFall) begin
            if (delayClocks == 3'h0 && !writing) begin
                writing <= 1'b1;
                addressLatch <= address;
            end
        end
        if (ieRise) begin
            delayClocks <= 3'h6;
            writing <= 1'b0;
            buffer[addressLatch] <= datap1;
        end
        
        if (writing) begin
            datap0 <= colour;
            datap1 <= datap0;
        end

        bgcolour[7:0] <= {buffer[13'h1FFF], buffer[13'h1FFE], buffer[13'h1FFD], buffer[13'h1FFC]};
        bgcolour[15:8] <= {buffer[13'h1FFB], buffer[13'h1FFA], buffer[13'h1FF9], buffer[13'h1FF8]};
        bgcolour[23:16] <= {buffer[13'h1FF7], buffer[13'h1FF6], buffer[13'h1FF5], buffer[13'h1FF4]};

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