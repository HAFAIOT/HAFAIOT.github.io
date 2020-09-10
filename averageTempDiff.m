% Send a warning useing thing speak if the tunnel is expected to overheat

% Channel IDs to read data from:
temperatureChannelID = ;            %Update
weatherChannelID = ;                %Update
averageChannelID = ;                %Update

% API Keys:
temperatureReadAPIKey = '';         %Update
weatherReadAPIKey = '';             %Update
averageReadAPIKey = '';             %Update
averageWriteAPIKey = '';            %Update

temperature = thingSpeakRead(temperatureChannelID,'NumDays',1,'OutputFormat','table');
weather = thingSpeakRead(weatherChannelID,'NumDays',1,'OutputFormat','table');
averageDifference = thingSpeakRead(averageChannelID);

temperature = rmmissing(temperature);
weather = rmmissing(weather);

weather = table2cell(weather);
temperature = table2cell(temperature);
[r1,c1]=size(weather)
[r2,c2]=size(temperature)


for i = 1:r1
    for j = 1:r2
        timeDiff=(posixtime(weather{i,1})-posixtime(temperature{j,1}));
        if(timeDiff<0)
            timeDiff=timeDiff*-1;
        end
        if(timeDiff<=300)
            averageDifference=((averageDifference*100+(temperature{j,2}-weather{i,2}))/101);
            break;
        end
    end
end

averageDifference

%% Write Data %%
thingSpeakWrite(averageChannelID,'Fields',1,'Values',averageDifference,'WriteKey',averageWriteAPIKey);
