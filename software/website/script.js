function showContent(contentId) 
{
    var allContents = document.querySelectorAll("#contentContainer > div");
    allContents.forEach(function(content) {
        content.style.display = "none";
    });

    var selectedContent = document.getElementById(contentId);
    if (selectedContent) {
        selectedContent.style.display = "block";
    }

    changeHeader()
}

function changeHeader()
{
    document.getElementById("headertext").html("PeeWee")
}

document.addEventListener("DOMContentLoaded", function() {
            showContent('content1', 'Header for Content 1');
        });
