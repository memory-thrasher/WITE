#include "stdafx.h"
#include "GPU.h"
#include "Export.h"

GPU::GPU(VkPhysicalDevice pdevice, size_t idx, size_t neededQueueCount, const unsigned int* neededQueues) :
		idx(idx), phys(pdevice) {
	unsigned int i, j;
	bool found;
	vkGetPhysicalDeviceProperties(phys, &props);
	vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &queueFamilyCount, NULL);
	if (queueFamilyCount > MAX_QUEUES)
		queueFamilyCount = MAX_QUEUES;
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &queueFamilyCount, queueFamProps);
	//TODO make a queue for each type needed
	unsigned int chosen[MAX_QUEUES], graphicsIdx = -1, computeIdx = -1, transferIdx = -1,
		graphicsComputeIdx = -1, graphicsTransferIdx = -1, computeTransferIdx = -1, allIdx = -1;
	bool graphics, compute, transfer;
	for (i = 0;i < queueFamilyCount && allIdx == -1;i++) {
		graphics = queueFamProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		compute = queueFamProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
		transfer = queueFamProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT;
		if (graphics) graphicsIdx = i;
		if (compute) computeIdx = i;
		if (transfer) transferIdx = i;
		if (graphics && compute) graphicsComputeIdx = i;
		if (graphics && transfer) graphicsTransferIdx = i;
		if (compute && transfer) computeTransferIdx = i;
		if (graphics && compute && transfer) allIdx = i;
	}
	queueCount = 3;
	if (allIdx != -1) {
		queueCount = 1;
		chosen[0] = graphicsIdx = computeIdx = transferIdx = allIdx;
	} else if (graphicsTransferIdx != -1) {
		queueCount = 2;
		chosen[0] = graphicsIdx = transferIdx = graphicsTransferIdx;
		chosen[1] = computeIdx;
	} else if (graphicsComputeIdx != -1) {
		queueCount = 2;
		chosen[0] = graphicsIdx = computeIdx = graphicsComputeIdx;
		chosen[1] = transferIdx;
	} else if (computeTransferIdx != -1) {
		queueCount = 2;
		chosen[1] = computeIdx = transferIdx = computeTransferIdx;
		chosen[0] = graphicsIdx;
	} else {
		chosen[0] = graphicsIdx;
		chosen[1] = transferIdx;
		chosen[2] = computeIdx;
	}
	for (i = 0;i < neededQueueCount;i++) {
		found = false;
		for (j = 0;j < queueCount && !found;j++)
			if (chosen[j] == neededQueues[i])
				found = true;
		if (!found)
			chosen[queueCount++] = neededQueues[i];
	}
	VkDeviceQueueCreateInfo *queueInfos =
		static_cast<VkDeviceQueueCreateInfo*>(malloc(queueCount * sizeof(VkDeviceQueueCreateInfo)));
	if (!queueInfos) CRASH;
	for(i = 0;i < queueCount;i++) {
		queueInfos[i] = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, chosen[i], 2,
			qPriorities + glm::min<size_t>(i, 3) * 2 };//TODO use min macro instead
	}
	VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, queueCount, queueInfos,
		0, NULL, DEVICE_EXTENSIONS_LEN, DEVICE_EXTENSIONS, NULL };
	CRASHIFFAIL(vkCreateDevice(phys, &deviceInfo, vkAlloc, &device));
	free(queueInfos);
	this->queueCount = queueCount * 2;
	for (i = 0;i < queueCount;i++) {
		queues[i] = new VQueue(this, chosen[i], 0);
		queues[i + queueCount] = new VQueue(this, chosen[i], 1);
	}
}


GPU::~GPU() {
	//TODO dispose stuff
}
