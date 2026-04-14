function stm32_live_gui
    clc;

    port = "COM3";      
    baud = 115200;

   
    s = serialport(port, baud);
    configureTerminator(s, "LF");
    flush(s);

    pause(2);
    writeline(s, "s");   % Start streaming immediately

    
    timeData   = [];
    u1Data     = [];
    u2Data     = [];
    angle1Data = [];
    angle2Data = [];

    t0 = tic;
    isRunning = true;
    isCleaned = false;

   
    fig = figure( ...
        'Name', 'STM32 Encoder Stream', ...
        'NumberTitle', 'off', ...
        'MenuBar', 'none', ...
        'ToolBar', 'figure', ...
        'Color', 'w', ...
        'CloseRequestFcn', @closeFigure);

    ax = axes('Parent', fig, 'Position', [0.08 0.32 0.88 0.63]);
    grid(ax, 'on');
    hold(ax, 'on');
    xlabel(ax, 'Time [s]');
    ylabel(ax, 'Angle [deg]');
    title(ax, 'Live encoder data');

   
    lineAngle2 = plot(ax, nan, nan, 'DisplayName', 'angle2');
    lineAngle1 = plot(ax, nan, nan, 'DisplayName', 'angle1');
    legend(ax, 'show');

    statusText = uicontrol( ...
        'Style', 'text', ...
        'Parent', fig, ...
        'Units', 'normalized', ...
        'Position', [0.08 0.22 0.88 0.05], ...
        'String', 'Waiting for data...', ...
        'BackgroundColor', 'w', ...
        'HorizontalAlignment', 'left', ...
        'FontSize', 10);

    uicontrol( ...
        'Style', 'text', ...
        'Parent', fig, ...
        'Units', 'normalized', ...
        'Position', [0.08 0.14 0.12 0.05], ...
        'String', 'Command:', ...
        'BackgroundColor', 'w', ...
        'HorizontalAlignment', 'left', ...
        'FontSize', 10);

    cmdEdit = uicontrol( ...
        'Style', 'edit', ...
        'Parent', fig, ...
        'Units', 'normalized', ...
        'Position', [0.20 0.14 0.22 0.06], ...
        'String', '', ...
        'FontSize', 11, ...
        'Callback', @sendCommandFromEdit);

    uicontrol( ...
        'Style', 'pushbutton', ...
        'Parent', fig, ...
        'Units', 'normalized', ...
        'Position', [0.44 0.14 0.12 0.06], ...
        'String', 'Send', ...
        'FontSize', 10, ...
        'Callback', @sendCommandFromButton);

    uicontrol( ...
        'Style', 'text', ...
        'Parent', fig, ...
        'Units', 'normalized', ...
        'Position', [0.08 0.04 0.88 0.07], ...
        'String', 'Allowed commands: 0..100, +, -, x, z, s, p, c', ...
        'BackgroundColor', 'w', ...
        'HorizontalAlignment', 'left', ...
        'FontSize', 10);


    while isRunning && isvalid(fig)
        try
            while s.NumBytesAvailable > 0
                line = readline(s);
                values = sscanf(line, '%f,%f,%f,%f');

                if numel(values) == 4
                    t = toc(t0);

                    u1 = values(1);
                    u2 = values(2);
                    angle1 = values(3);
                    angle2 = values(4);

                    timeData(end+1)   = t;
                    u1Data(end+1)     = u1;
                    u2Data(end+1)     = u2;
                    angle1Data(end+1) = angle1;
                    angle2Data(end+1) = angle2;

                    set(lineAngle2, 'XData', timeData, 'YData', angle2Data);
                    set(lineAngle1, 'XData', timeData, 'YData', angle1Data);

                    statusText.String = sprintf( ...
                        'u1 = %.2f | u2 = %.2f | angle1 = %.2f deg | angle2 = %.2f deg', ...
                        u1, u2, angle1, angle2);
                end
            end

            drawnow;
            pause(0.01);

        catch err
            fprintf(2, 'Runtime error: %s\n', err.message);
            break;
        end
    end

    cleanupSerial();

   
    function sendCommandFromButton(~, ~)
        sendCommand();
    end

    function sendCommandFromEdit(~, ~)
        sendCommand();
    end

    function sendCommand()
        try
            if ~isvalid(fig)
                return;
            end

            cmd = strtrim(get(cmdEdit, 'String'));

            if isempty(cmd)
                return;
            end

            writeline(s, cmd);
            set(cmdEdit, 'String', '');

        catch err
            fprintf(2, 'Failed to send command: %s\n', err.message);
        end
    end

    function closeFigure(~, ~)
        isRunning = false;
        cleanupSerial();

        if isvalid(fig)
            delete(fig);
        end
    end

    function cleanupSerial()
        if isCleaned
            return;
        end

        isCleaned = true;

        try
            if ~isempty(s)
                writeline(s, "p");
                pause(0.1);
            end
        catch
        end

        try
            delete(s);
        catch
        end
    end
end