const productTableBody = document.getElementById("product-table-body");
const scanTableBody = document.getElementById("scan-table-body");
const shoppingTableBody = document.getElementById("shopping-table-body");

const noticeBox = document.getElementById("notice");

const productIdInput = document.getElementById("product-id");
const barcodeInput = document.getElementById("barcode");
const nameInput = document.getElementById("name");
const qtLabelInput = document.getElementById("qt_label");
const specInput = document.getElementById("spec");
const priceInput = document.getElementById("price");
const enabledInput = document.getElementById("enabled");

const productForm = document.getElementById("product-form");
const resetFormBtn = document.getElementById("reset-form-btn");

const startShoppingBtn = document.getElementById("start-shopping-btn");
const resetShoppingBtn = document.getElementById("reset-shopping-btn");
const currentTotalEl = document.getElementById("current-total");
const checkoutTotalEl = document.getElementById("checkout-total");

let shoppingStarted = false;
let cartItems = [];
let lastProcessedScanId = 0;
let latestScanRows = [];

function escapeHtml(text) {
	if (text === null || text === undefined) return "";
	return String(text)
		.replaceAll("&", "&amp;")
		.replaceAll("<", "&lt;")
		.replaceAll(">", "&gt;")
		.replaceAll('"', "&quot;")
		.replaceAll("'", "&#039;");
}

function showNotice(message, type = "success") {
	if (!noticeBox) return;

	noticeBox.className = `notice ${type}`;
	noticeBox.textContent = message;
	noticeBox.classList.remove("hidden");

	setTimeout(() => {
		noticeBox.classList.add("hidden");
	}, 2500);
}

async function requestJson(url, options = {}) {
	const opts = { ...options };

	if (opts.body && typeof opts.body !== "string") {
		opts.body = JSON.stringify(opts.body);
	}

	opts.headers = {
		"Content-Type": "application/json",
		...(opts.headers || {}),
	};

	const resp = await fetch(url, opts);

	if (!resp.ok) {
		let msg = "请求失败";
		try {
			const data = await resp.json();
			msg = data.detail || JSON.stringify(data);
		} catch (e) {}
		throw new Error(msg);
	}

	if (resp.status === 204) {
		return null;
	}

	return resp.json();
}

function formatMoney(value) {
	return `¥${Number(value || 0).toFixed(2)}`;
}

function resetForm() {
	productIdInput.value = "";
	barcodeInput.value = "";
	nameInput.value = "";
	qtLabelInput.value = "";
	specInput.value = "";
	priceInput.value = "";
	enabledInput.checked = true;
}

function getProductByBarcode(barcode) {
	const list = window.__products || [];
	return list.find(item => String(item.barcode).trim() === String(barcode).trim());
}

function renderShoppingCart() {
	if (!shoppingTableBody) return;

	let total = 0;

	if (cartItems.length === 0) {
		shoppingTableBody.innerHTML = `
			<tr>
				<td colspan="7" class="empty-row">暂无商品</td>
			</tr>
		`;
		currentTotalEl.textContent = formatMoney(0);
		checkoutTotalEl.textContent = formatMoney(0);
		return;
	}

	shoppingTableBody.innerHTML = cartItems.map(item => {
		const subtotal = Number(item.price || 0) * item.qty;
		total += subtotal;

		return `
			<tr>
				<td>${escapeHtml(item.barcode)}</td>
				<td>${escapeHtml(item.name)}</td>
				<td>${escapeHtml(item.qt_label)}</td>
				<td>${formatMoney(item.price)}</td>
				<td>${item.qty}</td>
				<td>${formatMoney(subtotal)}</td>
				<td>
					<button class="btn" onclick="increaseQty(${item.id})">+</button>
					<button class="btn" onclick="decreaseQty(${item.id})">-</button>
					<button class="btn danger" onclick="removeFromCart(${item.id})">移除</button>
				</td>
			</tr>
		`;
	}).join("");

	currentTotalEl.textContent = formatMoney(total);
	checkoutTotalEl.textContent = formatMoney(total);
}

function addProductToCartByBarcode(barcode) {
	const p = getProductByBarcode(barcode);
	if (!p) return false;

	const exists = cartItems.find(x => x.id === p.id);
	if (exists) {
		exists.qty += 1;
	} else {
		cartItems.push({
			id: p.id,
			barcode: p.barcode,
			name: p.name,
			qt_label: p.qt_label,
			price: p.price || 0,
			qty: 1,
		});
	}

	renderShoppingCart();
	return true;
}

function processNewScansForShopping(scans) {
	if (!shoppingStarted) return;
	if (!Array.isArray(scans) || scans.length === 0) return;

	const ordered = [...scans].sort((a, b) => a.id - b.id);

	for (const scan of ordered) {
		if (!scan || !scan.id) continue;

		if (scan.id <= lastProcessedScanId) {
			continue;
		}

		if (scan.matched && scan.barcode && scan.barcode !== "NOT_FOUND") {
			const ok = addProductToCartByBarcode(scan.barcode);
			if (ok) {
				showNotice(`已自动加入商品：${scan.product_name || scan.barcode}`);
			}
		}

		lastProcessedScanId = scan.id;
	}
}

window.increaseQty = function (id) {
	const item = cartItems.find(x => x.id === id);
	if (!item) return;
	item.qty += 1;
	renderShoppingCart();
};

window.decreaseQty = function (id) {
	const item = cartItems.find(x => x.id === id);
	if (!item) return;

	item.qty -= 1;
	if (item.qty <= 0) {
		cartItems = cartItems.filter(x => x.id !== id);
	}
	renderShoppingCart();
};

window.removeFromCart = function (id) {
	cartItems = cartItems.filter(x => x.id !== id);
	renderShoppingCart();
};

window.addToCart = function (id) {
	if (!shoppingStarted) {
		showNotice("请先点击“开始购物”", "error");
		return;
	}

	const list = window.__products || [];
	const p = list.find(item => item.id === id);
	if (!p) return;

	const exists = cartItems.find(x => x.id === id);
	if (exists) {
		exists.qty += 1;
	} else {
		cartItems.push({
			id: p.id,
			barcode: p.barcode,
			name: p.name,
			qt_label: p.qt_label,
			price: p.price || 0,
			qty: 1,
		});
	}

	renderShoppingCart();
	showNotice(`已加入商品清单：${p.name}`);
};

async function loadStatus() {
	try {
		const data = await requestJson("/api/status");

		document.getElementById("mqtt-connected").textContent = data.connected ? "已连接" : "未连接";
		document.getElementById("mqtt-host").textContent = `${data.host}:${data.port}`;
		document.getElementById("mqtt-topic-result").textContent = data.topic_result;
		document.getElementById("mqtt-topic-cmd").textContent = data.topic_cmd;
		document.getElementById("mqtt-last-time").textContent = data.last_message_at || "--";
		document.getElementById("mqtt-last-error").textContent = data.last_error || "--";
		document.getElementById("last-incoming").textContent = data.last_incoming_payload || "--";
		document.getElementById("last-outgoing").textContent = data.last_outgoing_payload || "--";
	} catch (err) {
		console.error(err);
	}
}

async function loadProducts() {
	try {
		const products = await requestJson("/api/products");

		productTableBody.innerHTML = products.map(p => `
			<tr>
				<td>${p.id}</td>
				<td>${escapeHtml(p.barcode)}</td>
				<td>${escapeHtml(p.name)}</td>
				<td>${escapeHtml(p.qt_label)}</td>
				<td>${escapeHtml(p.spec || "")}</td>
				<td>${p.price === null ? "" : formatMoney(p.price)}</td>
				<td>${p.enabled ? '<span class="tag-ok">是</span>' : '<span class="tag-bad">否</span>'}</td>
				<td>
					<button class="btn" onclick="editProduct(${p.id})">编辑</button>
					<button class="btn success" onclick="publishProduct(${p.id})">发给QT</button>
					<button class="btn primary" onclick="addToCart(${p.id})">加入清单</button>
					<button class="btn danger" onclick="deleteProduct(${p.id})">删除</button>
				</td>
			</tr>
		`).join("");

		window.__products = products;
	} catch (err) {
		console.error(err);
	}
}

async function loadScans() {
	try {
		const scans = await requestJson("/api/scans?limit=50");
		latestScanRows = scans || [];

		scanTableBody.innerHTML = latestScanRows.map(s => `
			<tr>
				<td>${s.id}</td>
				<td>${escapeHtml(s.created_at)}</td>
				<td>${escapeHtml(s.barcode)}</td>
				<td>${escapeHtml(s.status)}</td>
				<td>${escapeHtml(s.source_status || "")}</td>
				<td>${escapeHtml(s.product_name || "")}</td>
				<td class="code-cell">${escapeHtml(s.qt_payload || "")}</td>
				<td class="code-cell">${escapeHtml(s.raw_payload)}</td>
			</tr>
		`).join("");

		processNewScansForShopping(latestScanRows);
	} catch (err) {
		console.error(err);
	}
}

window.editProduct = function (id) {
	const list = window.__products || [];
	const p = list.find(item => item.id === id);
	if (!p) return;

	productIdInput.value = p.id;
	barcodeInput.value = p.barcode || "";
	nameInput.value = p.name || "";
	qtLabelInput.value = p.qt_label || "";
	specInput.value = p.spec || "";
	priceInput.value = p.price ?? "";
	enabledInput.checked = !!p.enabled;
};

window.deleteProduct = async function (id) {
	if (!confirm("确认删除这个商品吗？")) return;

	try {
		await requestJson(`/api/products/${id}`, { method: "DELETE" });
		showNotice("删除成功");
		await loadProducts();
	} catch (err) {
		showNotice(err.message, "error");
	}
};

window.publishProduct = async function (id) {
	try {
		const data = await requestJson(`/api/products/${id}/publish`, { method: "POST" });
		showNotice(`已发送到QT：${data.payload}`);
		await loadStatus();
	} catch (err) {
		showNotice(err.message, "error");
	}
};

productForm.addEventListener("submit", async function (e) {
	e.preventDefault();

	const payload = {
		barcode: barcodeInput.value.trim(),
		name: nameInput.value.trim(),
		qt_label: qtLabelInput.value.trim(),
		spec: specInput.value.trim(),
		price: priceInput.value.trim() === "" ? null : Number(priceInput.value),
		enabled: enabledInput.checked,
	};

	try {
		const id = productIdInput.value.trim();
		if (id) {
			await requestJson(`/api/products/${id}`, {
				method: "PUT",
				body: payload,
			});
			showNotice("更新成功");
		} else {
			await requestJson("/api/products", {
				method: "POST",
				body: payload,
			});
			showNotice("新增成功");
		}

		resetForm();
		await loadProducts();
	} catch (err) {
		showNotice(err.message, "error");
	}
});

resetFormBtn.addEventListener("click", resetForm);

if (startShoppingBtn) {
	startShoppingBtn.addEventListener("click", function () {
		shoppingStarted = true;
		cartItems = [];

		if (Array.isArray(latestScanRows) && latestScanRows.length > 0) {
			lastProcessedScanId = Math.max(...latestScanRows.map(x => x.id || 0));
		} else {
			lastProcessedScanId = 0;
		}

		renderShoppingCart();
		showNotice("已开始购物，后续扫描商品将自动加入清单");
	});
}

if (resetShoppingBtn) {
	resetShoppingBtn.addEventListener("click", function () {
		cartItems = [];
		shoppingStarted = false;
		lastProcessedScanId = 0;
		renderShoppingCart();
		showNotice("购物清单已重置");
	});
}

async function initPage() {
	await loadStatus();
	await loadProducts();
	await loadScans();

	renderShoppingCart();

	setInterval(loadStatus, 3000);
	setInterval(loadScans, 3000);
	setInterval(loadProducts, 5000);
}

initPage();